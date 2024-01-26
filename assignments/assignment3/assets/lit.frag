#version 450
out vec4 FragColor; //The color of this fragment
in Surface{
	vec3 WorldPos; //Vertex position in world space
	vec2 TexCoord;
	mat3 TBN;
	vec4 LightSpacePos; //Clip space position in light space
}fs_in;

uniform sampler2D _MainTex; 
uniform vec3 _EyePos;
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);

struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;
uniform sampler2D _NormalMap;

struct Light{
	vec3 direction;
	vec3 color;
};
uniform Light _MainLight;

uniform sampler2D _ShadowMap;
uniform float _MinBias = 0.005; 
uniform float _MaxBias = 0.015;

//Returns 0 in shadow, 1 out of shadow
float calcShadow(vec3 normal, vec3 toLight){
	vec3 lightSpacePos = fs_in.LightSpacePos.xyz / fs_in.LightSpacePos.w;

	//[-1,1] to [0,1]
	lightSpacePos = lightSpacePos * 0.5f + 0.5f;

	//This fragment's depth from POV of the light source
	float myDepth = lightSpacePos.z;

	//Slope scale bias
	float bias = max(_MaxBias * (1.0 - dot(normal,toLight)),_MinBias);
	myDepth -= bias;

	//PCF
	float totalShadow = 0;
	vec2 texelOffset = 1.0 /  textureSize(_ShadowMap,0);
	for(int y = -1; y <=1; y++){
		for(int x = -1; x <=1; x++){
			vec2 uv = lightSpacePos.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow+=step(myDepth,texture(_ShadowMap,uv).r);
		}
	}
	totalShadow/=9.0;
	return totalShadow;
}

void main(){
	//Make sure fragment normal is still length 1 after interpolation.
	vec3 normal = texture(_NormalMap,fs_in.TexCoord).rgb * 2.0 - 1.0;
	normal = fs_in.TBN * normal;
	normal = normalize(normal);
	//vec3 normal = normalize(fs_in.WorldNormal);
	//Light pointing straight down
	vec3 toLight = -normalize(_MainLight.direction);
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * _MainLight.color;

	lightColor*=calcShadow(normal,toLight);

	lightColor+=_AmbientColor * _Material.Ka;
	vec3 objectColor = texture(_MainTex,fs_in.TexCoord).rgb;

	FragColor = vec4(objectColor * lightColor,1.0);
}

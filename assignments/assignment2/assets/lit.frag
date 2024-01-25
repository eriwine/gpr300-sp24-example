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

float calcShadow(){
	vec3 lightSpacePos = fs_in.LightSpacePos.xyz / fs_in.LightSpacePos.w;

	//[-1,1] to [0,1]
	lightSpacePos = lightSpacePos * 0.5f + 0.5f;

	//This fragment's depth from POV of the light source
	float myDepth = lightSpacePos.z;

	//Bias
	myDepth-=0.01;

	float shadowMapDepth = texture(_ShadowMap,lightSpacePos.xy).r;
	return step(myDepth,shadowMapDepth);
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

	lightColor*=calcShadow();

	lightColor+=_AmbientColor * _Material.Ka;
	vec3 objectColor = texture(_MainTex,fs_in.TexCoord).rgb;

	FragColor = vec4(objectColor * lightColor,1.0);
}

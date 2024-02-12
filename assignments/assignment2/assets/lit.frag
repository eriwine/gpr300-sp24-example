#version 450
out vec4 FragColor; //The color of this fragment

in Surface{
	vec3 WorldPos; //Vertex position in world space
	vec2 TexCoord;
	mat3 TBN;
	vec4 LightSpacePos; //Clip space position in light space
	vec4 SpotLightSpacePos; //Clip space position in spotlight space
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

struct SpotLight{
	vec3 direction;
	vec3 position;
	vec3 color;
	float radius;
	float cosMinAngle;
	float cosMaxAngle;
	sampler2D shadowMap;
};

uniform SpotLight _SpotLight;

uniform sampler2D _ShadowMap;
uniform float _MinBias = 0.005; 
uniform float _MaxBias = 0.015;

#define GOOD_SHADOWS

//Returns 0 in shadow, 1 out of shadow
float calcShadow(sampler2D shadowMap, vec3 normal, vec3 toLight, vec4 lightSpacePos){
	//vec3 lightSpacePos = fs_in.LightSpacePos.xyz / fs_in.LightSpacePos.w;
	lightSpacePos = lightSpacePos / lightSpacePos.w;

	//[-1,1] to [0,1]
	lightSpacePos = lightSpacePos * 0.5f + 0.5f;

	//This fragment's depth from POV of the light source
	float myDepth = lightSpacePos.z;

#ifdef GOOD_SHADOWS
	//Slope scale bias
	float bias = max(_MaxBias * (1.0 - dot(normal,toLight)),_MinBias);
	myDepth -= bias;

	//PCF
	float totalShadow = 0;
	vec2 texelOffset = 1.0 /  textureSize(shadowMap,0);
	for(int y = -1; y <=1; y++){
		for(int x = -1; x <=1; x++){
			vec2 uv = lightSpacePos.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow+=step(myDepth,texture(shadowMap,uv).r);
		}
	}
	totalShadow/=9.0;
	return totalShadow;
#else
	return step(myDepth,texture(shadowMap,lightSpacePos.xy).r);
#endif
}

vec3 calcLight(vec3 normal, vec3 lightColor, vec3 toLight, sampler2D shadowMap, vec4 lightSpacePos){

	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 color = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * lightColor;
	color*=calcShadow(shadowMap,normal,toLight,lightSpacePos);
	return color;
}

vec3 calcSpotLight(SpotLight light, vec3 normal){
	vec3 diff = light.position - fs_in.WorldPos;
	vec3 toLight = normalize(diff);
	vec3 color = calcLight(normal,light.color,toLight,light.shadowMap,fs_in.SpotLightSpacePos);

	//Linear attenuation
	float d = length(diff);
	float i = clamp(1.0 - pow((d / light.radius),4),0,1);
	i = i * i;

	//Anglular attenuation
	float cosLD = dot(light.direction,-toLight);
	i *= pow(clamp((cosLD - light.cosMaxAngle)/(light.cosMinAngle-light.cosMaxAngle),0,1),2);
	color *= i;
	return color;
}

void main(){
	//Make sure fragment normal is still length 1 after interpolation.
	vec3 normal = texture(_NormalMap,fs_in.TexCoord).rgb * 2.0 - 1.0;
	normal = fs_in.TBN * normal;
	normal = normalize(normal);

	vec3 lightColor = calcLight(normal, -normalize(_MainLight.direction), _MainLight.color, _ShadowMap, fs_in.LightSpacePos);
	lightColor += calcSpotLight(_SpotLight,normal);
	lightColor+=_AmbientColor * _Material.Ka;
	vec3 objectColor = texture(_MainTex,fs_in.TexCoord).rgb;

	FragColor = vec4(objectColor * lightColor,1.0);
}

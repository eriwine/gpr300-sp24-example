#version 450
out vec4 FragColor; //The color of this fragment

in vec2 UV;

uniform vec3 _EyePos;
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);

struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;

struct DirLight{
	vec3 direction;
	vec3 color;
};
uniform DirLight _MainLight;

uniform mat4 _LightTransform;

//Gbuffers
uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;

//Shadow mapping
uniform layout(binding = 3) sampler2D _ShadowMap;
uniform float _MinBias = 0.005; 
uniform float _MaxBias = 0.015;

//Point lights
struct PointLight{
	vec3 position;
	vec3 color;
	float radius;
};
#define MAX_POINT_LIGHTS 16
uniform PointLight _PointLights[MAX_POINT_LIGHTS];
uniform int _NumPointLights = MAX_POINT_LIGHTS;

vec3 calcPointLight(PointLight light, vec3 worldPos, vec3 normal){
	vec3 toLight = normalize(light.position - worldPos);
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - worldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * light.color;

	//Attenuation
	float d = length(light.position - worldPos);
	float i = clamp(1.0 - pow((d / light.radius),4),0,1);
	i = i * i;
	lightColor *= i;
	return lightColor;
}

//Returns 0 in shadow, 1 out of shadow
float calcShadow(vec4 lightSpacePos, vec3 normal, vec3 toLight){
	lightSpacePos = lightSpacePos / lightSpacePos.w;

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
	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 albedo = texture(_gAlbedo,UV).xyz;

	//vec3 normal = normalize(fs_in.WorldNormal);
	//Light pointing straight down
	vec3 toLight = -normalize(_MainLight.direction);
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - worldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * _MainLight.color;

	vec4 lightSpacePos = _LightTransform * vec4(worldPos,1);
	lightColor*=calcShadow(lightSpacePos,normal,toLight);

	lightColor+=_AmbientColor * _Material.Ka;

//	//Add all point lights
//	for(int i = 0; i < _NumPointLights; i++){
//		lightColor+=calcPointLight(_PointLights[i],worldPos,normal);
//	}
//
	FragColor = vec4(albedo * lightColor,1.0);
}

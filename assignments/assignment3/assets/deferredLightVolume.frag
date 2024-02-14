#version 450
out vec3 FragColor;

//Per instanced properties 
//in vec3 Color;
//in vec3 LightCenterPos;
//in float Radius;
in flat int InstanceID;

struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};

//Gbuffers
uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;

uniform vec3 _EyePos;

uniform Material _Material;
uniform vec2 _ScreenSize;

struct PointLight{
	vec3 position;
	float radius;
	vec3 color;
};
#define MAX_POINT_LIGHTS 1024
layout (std140, binding=0) uniform AdditionalLights{
	PointLight _PointLights[MAX_POINT_LIGHTS];
};

vec3 calcPointLight(vec3 worldPos, vec3 normal){

	vec3 Color = _PointLights[InstanceID].color;
	vec3 LightCenterPos = _PointLights[InstanceID].position;
	float Radius = _PointLights[InstanceID].radius;

	float d = length(LightCenterPos - worldPos);
	//if (d > Radius)
		//return vec3(0);

	vec3 toLight = normalize(LightCenterPos - worldPos);
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - worldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * Color;

	//Attenuation
	float i = clamp(1.0 - pow((d / Radius),4),0,1);
	i = i * i;
	lightColor *= i;
	return lightColor;
}


void main(){
	//0-1 UV for sampling gBuffers
    vec2 UV = gl_FragCoord.xy / textureSize(_gNormals,0); 
	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 lightColor = calcPointLight(worldPos,normal);
	FragColor = lightColor * texture(_gAlbedo,UV).rgb;
}
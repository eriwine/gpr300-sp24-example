#version 450
out vec4 FragColor;

//Point lights
struct PointLight{
	vec3 position;
	vec3 color;
	float radius;
};

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
uniform PointLight _PointLight;

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

uniform vec2 _ScreenSize;

void main(){
    vec2 UV = gl_FragCoord.xy / _ScreenSize; 
	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 albedo = texture(_gAlbedo,UV).xyz;
	vec3 lightColor = calcPointLight(_PointLight,worldPos,normal);
	FragColor = vec4(lightColor,1.0);
	//FragColor = vec4(_PointLight.color,1.0);
	//FragColor = vec4(UV,0.0,1.0);
}
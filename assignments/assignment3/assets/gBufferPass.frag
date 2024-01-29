#version 450
layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gAlbedo;

in Surface{
	vec3 WorldPos; //Vertex position in world space
	vec2 TexCoord;
	mat3 TBN;
}fs_in;

uniform layout(binding = 0) sampler2D _MainTex;
uniform layout(binding = 1) sampler2D _NormalMap;
uniform vec2 _Tiling = vec2(1.0);
void main(){
	gPosition = fs_in.WorldPos;
	gAlbedo.rgb = texture(_MainTex,fs_in.TexCoord * _Tiling).rgb;
	vec3 normal = texture(_NormalMap,fs_in.TexCoord * _Tiling).rgb * 2.0 - 1.0;
	normal = normalize(fs_in.TBN * normal);
	gNormal = normal;
}

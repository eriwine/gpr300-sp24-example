#version 450 core
//Vertex attributes
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;
layout(location = 3) in vec3 vTangent;

uniform mat4 _Model; 
uniform mat4 _ViewProjection;

out Surface{
	vec3 WorldPos; //Vertex position in world space
	vec2 TexCoord;
	mat3 TBN;
}vs_out;

void main(){
	//Transform vertex position to World Space.
	vs_out.WorldPos = vec3(_Model * vec4(vPos,1.0));
	//Transform vertex normal to world space using Normal Matrix
	vs_out.TexCoord = vTexCoord;
	vs_out.TBN =  transpose(inverse(mat3(_Model))) * mat3(vTangent,cross(vNormal,vTangent),vNormal);
	gl_Position = _ViewProjection * _Model * vec4(vPos,1.0);
}

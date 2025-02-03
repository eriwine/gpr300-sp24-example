#version 330

layout(location = 0) in vec3 vPos;

uniform mat4 _FrustumInvProj;
uniform mat4 _ViewProjection;

void main(){
	vec4 worldPos = _FrustumInvProj * vec4(vPos,1.0);
	gl_Position = _ViewProjection * worldPos;
}
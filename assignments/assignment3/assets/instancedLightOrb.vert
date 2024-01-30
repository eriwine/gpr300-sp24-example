#version 450
//Vertex attributes
layout(location = 0) in vec3 vPos;

//Per instance
layout(location = 4) in vec4 vInstancePosScale;
layout(location = 5) in vec3 vInstanceColor;

uniform mat4 _ViewProjection;

out vec3 Color;
void main(){
	vec4 worldPosition = vec4(vPos,1.0);
	worldPosition.xyz*=0.1;//vInstancePosScale.w * 0.1;
	worldPosition.xyz+=vInstancePosScale.xyz;

	gl_Position = _ViewProjection * worldPosition;

	Color = vInstanceColor;
}

#version 450 core
//Vertex attributes
layout(location = 0) in vec3 vPos;

layout(location = 4) in vec4 vInstancePosScale;
layout(location = 5) in vec3 vInstanceColor;

uniform mat4 _ViewProjection;

//Per light properties
out vec3 Color;
out vec3 LightCenterPos;
out float Radius;

void main(){
	vec4 worldPosition = vec4(vPos,1.0);
	worldPosition.xyz*=vInstancePosScale.w;
	worldPosition.xyz+=vInstancePosScale.xyz;

	gl_Position = _ViewProjection * worldPosition;

	Color = vInstanceColor;
	LightCenterPos = vInstancePosScale.xyz;//worldPosition.xyz;
	Radius = vInstancePosScale.w;
}

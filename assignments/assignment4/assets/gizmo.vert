#version 330 core
layout (location = 0) in vec3 vPos;

uniform mat4 _MVP;

void main(){
	gl_Position = _MVP * vec4(vPos,1);
}
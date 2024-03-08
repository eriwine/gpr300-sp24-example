#version 330 core
out vec4 FragColor;
uniform vec4 _Color = vec4(0,0,0,1);
void main(){
	FragColor = _Color;
}
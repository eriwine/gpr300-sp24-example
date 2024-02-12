#version 450 core
out vec4 FragColor;
in vec2 UV;
uniform sampler2D _ColorBuffer;

void main(){
	vec3 color = texture(_ColorBuffer,UV).rgb;
	//Gamma correction
	//color = pow(color ,vec3(1.0/2.2));
	FragColor = vec4(color,1.0);
}

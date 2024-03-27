//Invert effect fragment shader
#version 450
out vec4 FragColor;
in vec2 UV;
uniform sampler2D _ColorBuffer;

void main(){
	vec3 color = texture(_ColorBuffer,UV).rgb;
	color = pow(color ,vec3(1.0/2.2));
	//Gamma correction
	FragColor = vec4(color,1.0);
}

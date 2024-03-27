//Invert effect fragment shader
#version 450
out vec4 FragColor;
in vec2 UV;
uniform sampler2D _ColorBuffer;
uniform int _KernelSize = 9; //Must be odd
void main(){
	vec2 texelSize = 1.0 / textureSize(_ColorBuffer,0).xy;
	vec3 totalColor = vec3(0);
	int extents = _KernelSize/2;
	for(int y = -extents; y <= extents; y++){
		for(int x = -extents; x <= extents; x++){
			vec2 offset = vec2(x,y) * texelSize;
			totalColor += texture(_ColorBuffer,UV + offset).rgb;
		}
	}
	totalColor/=(_KernelSize * _KernelSize);
	FragColor = vec4(totalColor,1.0);
	
	//Gamma correction
	FragColor.rgb = pow(FragColor.rgb ,vec3(1.0/2.2));
}

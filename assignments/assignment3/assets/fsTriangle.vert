#version 450 core
out vec2 UV;

//vec4(X,Y,U,V)
vec4 vertices[3] = {
	vec4(-1,-1,0,0),//Bottom left
	vec4(3,-1,2,0), //Bottom right
	vec4(-1,3,0,2)  //Top left
};

void main(){
	UV = vertices[gl_VertexID].zw;
	gl_Position = vec4(vertices[gl_VertexID].xy,0,1);
}
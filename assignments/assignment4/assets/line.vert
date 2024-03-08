#version 450 
layout(std430, binding = 0) buffer TVertex
{
   vec4 vertex[];
};
uniform mat4 _ViewProjection;
uniform vec2 _ScreenResolution;
uniform float _Thickness; //In pixels
void main(){
    int line_i = gl_VertexID / 6; //Index of start of line segment
    int tri_i  = gl_VertexID % 6; //Index within quad

    //Get 4 points in window coordinates
    //va[1] is start point
    //va[2] is end point
	vec4 va[4];
    for (int i=0; i<4; ++i)
    {
        //[-w,w]
        va[i] = _ViewProjection * vertex[line_i+i];
        //[-1,1]
        va[i].xyz /= va[i].w;
        //[0,_ScreenResolution]
        va[i].xy = (va[i].xy + 1.0) * 0.5 * _ScreenResolution;
    }

    //Direction of line (end-start)
    vec2 v_line  = normalize(va[2].xy - va[1].xy);
    //Default tangent of line cap
    vec2 nv_line = vec2(-v_line.y, v_line.x);

    vec4 pos;

    //Vertices at start of line segment
    if (tri_i == 0 || tri_i == 1 || tri_i == 3)
    {
        vec2 v_pred  = normalize(va[1].xy - va[0].xy);
        vec2 v_miter = normalize(nv_line + vec2(-v_pred.y, v_pred.x));

        pos = va[1];
        pos.xy += v_miter * _Thickness * (tri_i == 1 ? -0.5 : 0.5) / dot(v_miter, nv_line);
    }
    else //Vertices at end of line segment
    {
        vec2 v_succ  = normalize(va[3].xy - va[2].xy);
        vec2 v_miter = normalize(nv_line + vec2(-v_succ.y, v_succ.x));

        pos = va[2];
        pos.xy += v_miter * _Thickness * (tri_i == 5 ? 0.5 : -0.5) / dot(v_miter, nv_line);
    }

    pos.xy = pos.xy / _ScreenResolution * 2.0 - 1.0;
    pos.xyz *= pos.w;
    gl_Position = pos;
}
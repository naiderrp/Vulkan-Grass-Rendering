#version 450

// we tessellate every vertex since each 
// blade is represented with one vertex 
// (v0) v1 and v2 are attributes
layout(vertices = 1) out;

layout(location = 0) in vec4 in_v0[];
layout(location = 1) in vec4 in_v1[];
layout(location = 2) in vec4 in_v2[];
layout(location = 3) in vec4 in_up[];

layout(location = 0) out vec4 out_v0[]; 
layout(location = 1) out vec4 out_v1[];
layout(location = 2) out vec4 out_v2[];
layout(location = 3) out vec4 out_up[];


void main() {
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

	out_v0[gl_InvocationID] = in_v0[gl_InvocationID];
	out_v1[gl_InvocationID] = in_v1[gl_InvocationID];
	out_v2[gl_InvocationID] = in_v2[gl_InvocationID];
	out_up[gl_InvocationID] = in_up[gl_InvocationID];

	/*
		ccw mode:

		gl_TessLevelOuter[0] = left;
		gl_TessLevelOuter[1] = bottom;
		gl_TessLevelOuter[2] = right;
		gl_TessLevelOuter[3] = top;

		gl_TessLevelInner[0] = top_bottom;
		gl_TessLevelOuter[1] = left_right;
	*/

	gl_TessLevelInner[0] = 10.0;
    gl_TessLevelInner[1] = 10.0;
    gl_TessLevelOuter[0] = 10.0;
    gl_TessLevelOuter[1] = 10.0;
    gl_TessLevelOuter[2] = 10.0;
    gl_TessLevelOuter[3] = 10.0;
}
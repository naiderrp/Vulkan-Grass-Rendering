#version 450

layout(location = 0) in vec4 in_v0;
layout(location = 1) in vec4 in_v1;
layout(location = 2) in vec4 in_v2;
layout(location = 3) in vec4 in_up;

layout(location = 0) out vec4 out_v0; 
layout(location = 1) out vec4 out_v1;
layout(location = 2) out vec4 out_v2;
layout(location = 3) out vec4 out_up;

layout(push_constant) uniform push_data {
	mat4 model_matrix;
} push;

void main() {

	out_v0 = vec4((push.model_matrix * vec4(in_v0.xyz, 1.0f)).xyz, in_v0.w);
	out_v1 = vec4((push.model_matrix * vec4(in_v1.xyz, 1.0f)).xyz, in_v1.w);
	out_v2 = vec4((push.model_matrix * vec4(in_v2.xyz, 1.0f)).xyz, in_v2.w);
	out_up = vec4(normalize(out_v1 - out_v0).xyz, 0.0f); //in_up.w is stiffness

	gl_Position = vec4(out_v0.xyz, 1.0f);
}
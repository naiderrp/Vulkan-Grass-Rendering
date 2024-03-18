#version 450

layout( quads, equal_spacing, ccw) in;

layout(location = 0) in vec4 in_v0[];
layout(location = 1) in vec4 in_v1[];
layout(location = 2) in vec4 in_v2[];
layout(location = 3) in vec4 in_up[];

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;

layout(push_constant) uniform push_data {
	layout(offset = 64)
	mat4 view_matrix;
	mat4 projection_matrix;
} push;

void main() {

	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y;

	vec3 v0 = gl_in[0].gl_Position.xyz;
	vec3 v1 = in_v1[0].xyz;
	vec3 v2 = in_v2[0].xyz;

	vec3 n = in_up[0].xyz;
	
	float width = in_v2[0].w;
	float direction_angle = in_v0[0].w;
	
	vec3 t1 = vec3(-cos(direction_angle), 0.0, sin(direction_angle));
	
	vec3 a = v0 + v * (v1 - v0); // amount up
	vec3 b = v1 + v * (v2 - v1); // amount forward
	vec3 c = a + v * (b - a);
	
	vec3 c0 = c - width * t1;
	vec3 c1 = c + width * t1;

	float t = u + 0.5f * v - u * v;
	//float t = u;
	
	//float threshold = 0.35;
	//float t = 0.5 + (u - 0.5) * (1 - max(v - threshold, 0)/(1 - threshold));
	
	vec3 p = (1 - t) * c0 + t * c1;

	//vec3 p = mix(c0, c1, t);
    
	gl_Position = push.projection_matrix * push.view_matrix * vec4(p, 1.0f);
	
	vec3 t0 = normalize(b - a);
	normal = vec4(normalize(cross(t0, t1)), 0.0);

	position = in_v0[0];
}
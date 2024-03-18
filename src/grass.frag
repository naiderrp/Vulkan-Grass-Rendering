#version 450

layout(location = 0) out vec4 frag_color;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;

void main() {
	float diffuse = 0.33 + clamp(dot(normal.xyz, vec3(0, -1, 0)), 0, 0.67); //lambert at noon
	vec4 green = vec4(0.0f, 1.0f, 0.0f, 1.0f);
	frag_color = diffuse * green;
}
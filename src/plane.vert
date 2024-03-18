#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(push_constant) uniform push_data {
	mat4 model_matrix;
	mat4 view_matrix;
	mat4 projection_matrix;
} push;

void main() {
	gl_Position = push.projection_matrix * push.view_matrix * push.model_matrix * vec4(inPosition, 1.0); 
	
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}
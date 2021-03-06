#version 330 core

uniform mat4 model;

layout (location = 0) in vec3 vpos;

void main() {
	gl_Position = model * vec4(vpos, 1.0);
}

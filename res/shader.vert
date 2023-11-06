#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in mat4 model;
layout(location = 6) in mat4 view;
layout(location = 10) in mat4 proj;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = proj * view * model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
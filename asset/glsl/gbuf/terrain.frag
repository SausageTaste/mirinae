#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(vec3(-4.0, -4.0, 0.0));

    outFragColor.rgb = vec3(1, 0, 0);
}

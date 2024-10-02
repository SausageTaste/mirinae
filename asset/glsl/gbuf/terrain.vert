#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;


void main(void) {
    gl_Position = vec4(i_pos.xyz, 1.0);
    outNormal = i_normal;
    outUV = i_texcoord;
}

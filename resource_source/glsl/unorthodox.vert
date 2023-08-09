#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;

layout(location = 0) out vec3 v_frag_color;


void main() {
    gl_Position = vec4(i_pos, 1);
    v_frag_color = i_normal;
}

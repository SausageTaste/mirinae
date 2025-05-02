#version 450

layout(location = 0) in vec4 i_pos;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 v_color;


void main() {
    gl_Position = i_pos;
    v_color = i_color;
}

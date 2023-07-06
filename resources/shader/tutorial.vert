#version 450 core

layout (location = 0) in vec3 i_pos;

uniform mat4 u_proj_mat;


void main() {
    vec4 world_pos = u_proj_mat * vec4(i_pos, 1);
    gl_Position = world_pos;
}

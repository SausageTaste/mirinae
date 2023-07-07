#version 450 core

layout (location = 0) in vec3 i_pos;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uv;

out vec3 v_normal;
out vec2 v_uv;

uniform mat4 u_proj_mat;


void main() {
    vec4 world_pos = u_proj_mat * vec4(i_pos, 1);
    gl_Position = world_pos;
    v_normal = i_normal;
    v_uv = i_uv;
}

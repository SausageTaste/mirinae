#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texcoord;

layout(location = 0) out vec3 v_frag_color;
layout(location = 1) out vec2 v_texcoord;


layout(binding = 0) uniform U_Unorthodox {
    mat4 model;
    mat4 view;
    mat4 proj;
} u_unorthodox;


void main() {
    mat4 mvp = u_unorthodox.proj * u_unorthodox.view * u_unorthodox.model;
    gl_Position = mvp * vec4(i_pos, 1);
    v_frag_color = i_normal;
    v_texcoord = i_texcoord;
}

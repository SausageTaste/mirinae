#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texcoord;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_world_pos;
layout(location = 2) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufModel {
    mat4 model;
    mat4 view;
    mat4 proj;
} u_gbuf_model;


void main() {
    vec4 world_pos = u_gbuf_model.model * vec4(i_pos, 1);
    gl_Position = (u_gbuf_model.proj * u_gbuf_model.view) * world_pos;

    v_normal = i_normal;
    v_world_pos = world_pos.xyz;
    v_texcoord = i_texcoord;
}

#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texcoord;
layout(location = 3) in vec4 i_jweights;
layout(location = 4) in ivec4 i_jids;

layout(location = 0) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufActor {
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;


void main() {
    gl_Position = u_gbuf_model.pvm * vec4(i_pos, 1);
    v_normal = i_normal;
    v_texcoord = i_texcoord;
}

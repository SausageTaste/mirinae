#version 450

#include "../utils/normal_mapping.glsl"

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;

layout(location = 0) out mat3 v_tbn;
layout(location = 3) out vec3 v_frag_pos;
layout(location = 4) out vec2 v_texcoord;


layout(set = 2, binding = 0) uniform U_GbufActor {
    mat4 model;
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;


void main() {
    vec4 view_space_pos = u_gbuf_model.view_model * vec4(i_pos, 1);

    gl_Position = u_gbuf_model.pvm * vec4(i_pos, 1);
    v_tbn = make_tbn_mat(i_normal, i_tangent, u_gbuf_model.view_model);
    v_frag_pos = view_space_pos.xyz;
    v_texcoord = i_texcoord;
}

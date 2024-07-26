#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;

layout(location = 0) out mat3 v_tbn;
layout(location = 3) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufActor {
    mat4 model;
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;


void main() {
    gl_Position = u_gbuf_model.pvm * vec4(i_pos, 1);
    v_tbn = mat3(u_gbuf_model.view_model) * mat3(i_tangent, normalize(cross(i_normal, i_tangent)), i_normal);
    v_texcoord = i_texcoord;
}

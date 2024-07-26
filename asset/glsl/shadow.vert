#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;


layout(push_constant) uniform U_ShadowPushConst {
    mat4 pvm;
} u_push_const;

layout(set = 0, binding = 0) uniform U_GbufActor {
    mat4 model;
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;


void main() {
    gl_Position = u_push_const.pvm * vec4(i_pos, 1);
}

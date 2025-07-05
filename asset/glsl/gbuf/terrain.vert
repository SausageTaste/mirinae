#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec2 i_texco;

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec2 v_uv;


layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 pvm_;
    mat4 view_model_;
    vec2 fbuf_size_;
    vec2 len_per_texel_;
    float height_scale_;
    float tess_factor_;
} u_pc;


const vec2[] POSITIONS = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    gl_Position.xyz = i_pos;
    gl_Position.w = 1;
    v_uv = i_texco;
    v_normal = vec3(0, 1, 0);
}

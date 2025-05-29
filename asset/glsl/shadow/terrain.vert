#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec2 i_texco;

layout (location = 0) out vec2 v_uv;


layout (push_constant) uniform U_ShadowTerrainPushConst {
    mat4 pvm;
    vec4 height_map_size_fbuf_size;
    vec4 terrain_size;
    float height_scale;
    float tess_factor;
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
}

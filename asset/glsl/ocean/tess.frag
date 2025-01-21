#version 450

#include "../utils/lighting.glsl"

layout (location = 0) in vec3 i_frag_pos;
layout (location = 1) in vec2 i_uv;

layout (location = 0) out vec4 f_color;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    float height_scale;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;
layout(set = 0, binding = 1) uniform sampler2D u_normal_map;


void main() {
    const vec3 normal_texel = textureLod(u_normal_map, i_uv, 0).xyz;
    const vec3 normal = mat3(u_pc.view) * (normal_texel * 2 - 1);
    const vec3 albedo = vec3(0.1, 0.15, 0.25);
    const float roughness = 0.1;
    const float metallic = 0;

    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 light = vec3(0);

    const vec3 light_dir = mat3(u_pc.view) *  normalize(vec3(1, 1, 0));
    light += calc_pbr_illumination(
        0.2,
        0,
        albedo,
        normal,
        F0,
        -normalize(i_frag_pos),
        light_dir,
        vec3(3)
    );

    f_color.xyz = light;
    f_color.w = 1;
}

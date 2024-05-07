#version 450

#include "utils/lighting.glsl"

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec3 v_frag_pos;
layout(location = 4) in vec2 v_texcoord;

layout(location = 0) out vec4 out_composition;


layout(set = 0, binding = 0) uniform U_TranspFrame {
    mat4 proj_inv;

    // Directional light
    vec4 dlight_dir;
    vec4 dlight_color;

    // Spotlight
    vec4 slight_pos_n_inner_angle;
    vec4 slight_dir_n_outer_angle;
    vec4 slight_color_n_max_dist;
} u_comp_main;

layout(set = 1, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 1, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 1, binding = 2) uniform sampler2D u_normal_map;


void main() {
    vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    vec4 normal_texel = texture(u_normal_map, v_texcoord);

    const vec3 frag_pos = v_frag_pos;
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(v_tbn * (normal_texel.xyz * 2 - 1));
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;

    const vec3 view_direc = normalize(frag_pos);
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);

    vec3 light = albedo_texel.rgb * 0.2;

    // Directional light
    {
        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            u_comp_main.dlight_dir.xyz,
            u_comp_main.dlight_color.rgb
        );
    }

    // Flashlight
    {
        const vec3 light_pos = u_comp_main.slight_pos_n_inner_angle.xyz;
        const vec3 to_light = normalize(light_pos - frag_pos);
        const vec3 light_dir = u_comp_main.slight_dir_n_outer_angle.xyz;

        const float attenuation = calc_slight_attenuation(
            frag_pos,
            light_pos,
            light_dir,
            u_comp_main.slight_pos_n_inner_angle.w,
            u_comp_main.slight_dir_n_outer_angle.w
        ) * calc_attenuation(
            frag_distance, u_comp_main.slight_color_n_max_dist.w
        );

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            to_light,
            u_comp_main.slight_color_n_max_dist.xyz
        ) * attenuation;
    }

    out_composition.rgb = light;
    out_composition.a = albedo_texel.a;
}

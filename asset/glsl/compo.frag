#version 450

#include "utils/lighting.glsl"
#include "utils/shadow.glsl"

layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_depth_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;
layout(set = 0, binding = 3) uniform sampler2D u_material_map;

layout(set = 0, binding = 4) uniform U_CompoMain {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;

    // Directional light
    mat4 dlight_mat;
    vec4 dlight_dir;
    vec4 dlight_color;

    // Spotlight
    mat4 slight_mat;
    vec4 slight_pos_n_inner_angle;
    vec4 slight_dir_n_outer_angle;
    vec4 slight_color_n_max_dist;
} u_comp_main;

layout(set = 0, binding = 5) uniform sampler2D u_dlight_shadow_map;
layout(set = 0, binding = 6) uniform sampler2D u_slight_shadow_map;
layout(set = 0, binding = 7) uniform samplerCube u_envmap;


vec3 calc_frag_pos(float depth) {
    vec4 clip_pos = vec4(v_uv_coord * 2 - 1, depth, 1);
    vec4 frag_pos = u_comp_main.proj_inv * clip_pos;
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos = calc_frag_pos(depth_texel);
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.x;
    const float metallic = material_texel.y;

    const vec3 world_pos = (u_comp_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const vec3 reflect_direc = reflect(view_direc, normal);
    const vec3 world_reflect = (u_comp_main.view_inv * vec4(reflect_direc, 0)).xyz;
    const vec3 envmap_texel = texture(u_envmap, world_reflect).xyz;

    vec3 light = albedo_texel.rgb * 0.4;

    // Directional light
    light += calc_pbr_illumination(
        roughness,
        metallic,
        albedo,
        normal,
        F0,
        -view_direc,
        u_comp_main.dlight_dir.xyz,
        u_comp_main.dlight_color.rgb
    ) * how_much_not_in_shadow_pcf_bilinear(world_pos, u_comp_main.dlight_mat, u_dlight_shadow_map);

    // Flashlight
    {
        const vec3 light_pos = u_comp_main.slight_pos_n_inner_angle.xyz;
        const vec3 to_light = normalize(light_pos - frag_pos);
        const vec3 to_light_dir = u_comp_main.slight_dir_n_outer_angle.xyz;

        const float attenuation = calc_slight_attenuation(
            frag_pos,
            light_pos,
            -to_light_dir,
            u_comp_main.slight_pos_n_inner_angle.w,
            u_comp_main.slight_dir_n_outer_angle.w
        ) * calc_attenuation(
            frag_distance, u_comp_main.slight_color_n_max_dist.w
        );

        const float not_shadow = how_much_not_in_shadow_pcf_bilinear(
            world_pos, u_comp_main.slight_mat, u_slight_shadow_map
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
        ) * attenuation * not_shadow;
    }

    f_color = vec4(light, 1);
    f_color.xyz = mix(f_color.xyz, envmap_texel, 0.5);
}

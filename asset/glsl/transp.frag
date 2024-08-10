#version 450

#include "utils/lighting.glsl"
#include "utils/shadow.glsl"

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec3 v_frag_pos;
layout(location = 4) in vec2 v_texcoord;

layout(location = 0) out vec4 out_compo;


layout(set = 0, binding = 0) uniform U_TranspFrame {
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

layout(set = 0, binding = 1) uniform sampler2D u_dlight_shadow_map;
layout(set = 0, binding = 2) uniform sampler2D u_slight_shadow_map;
layout(set = 0, binding = 3) uniform samplerCube u_envmap;

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

    const vec3 world_pos = (u_comp_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 world_normal = (u_comp_main.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);

    vec3 light = vec3(0);

    {
        float NdotV = dot(normal, view_direc);
        vec3 kS = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
        vec3 kD = vec3(1.0) - F0;
        vec3 diffuse = texture(u_envmap, world_normal).xyz * albedo.xyz;
        light += kD * diffuse;
    }

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

    out_compo.rgb = light;
    out_compo.a = albedo_texel.a;
}

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
    mat4 dlight_mats[4];
    vec4 dlight_dir;
    vec4 dlight_color;
    vec4 dlight_cascade_depths;

    // Spotlight
    mat4 slight_mat;
    vec4 slight_pos_n_inner_angle;
    vec4 slight_dir_n_outer_angle;
    vec4 slight_color_n_max_dist;
} u_comp_main;

layout(set = 0, binding = 1) uniform sampler2D u_dlight_shadow_map;
layout(set = 0, binding = 2) uniform sampler2D u_slight_shadow_map;
layout(set = 0, binding = 3) uniform samplerCube u_env_diffuse;
layout(set = 0, binding = 4) uniform samplerCube u_env_specular;
layout(set = 0, binding = 5) uniform sampler2D u_env_lut;

layout(set = 1, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 1, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 1, binding = 2) uniform sampler2D u_normal_map;


vec3 ibl(
    const vec3 normal,
    const vec3 view_direc,
    const vec3 albedo,
    const vec3 f0,
    const float roughness,
    const float metallic
) {
    const vec3 N = normalize(normal);
    const vec3 V = -normalize(view_direc);
    const vec3 R = reflect(-V, N);
    const float NoV = max(dot(N, V), 0.0);
    const vec3 F = fresnel_schlick_rughness(NoV, f0, roughness);

    const vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const vec3 diffuse = texture(u_env_diffuse, N).rgb * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    const float mip_lvl = roughness * MAX_REFLECTION_LOD;
    const vec3 prefiltered_color = textureLod(u_env_specular, R, mip_lvl).rgb;
    const vec2 env_brdf = texture(u_env_lut, vec2(NoV, roughness)).rg;
    const vec3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);

    return kD * diffuse + specular;
}


void main() {
    vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    vec4 normal_texel = texture(u_normal_map, v_texcoord);

    const float depth = gl_FragCoord.z;
    const vec3 frag_pos = v_frag_pos;
    const vec3 albedo = albedo_texel.rgb;
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;

    vec3 normal = normalize(v_tbn * (normal_texel.xyz * 2 - 1));
    if (normal.z < 0)
        normal = -normal;

    const vec3 world_pos = (u_comp_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 world_normal = (u_comp_main.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 world_direc = (u_comp_main.view_inv * vec4(view_direc, 0)).xyz;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const vec3 reflect_direc = reflect(view_direc, normal);
    const vec3 world_reflect = (u_comp_main.view_inv * vec4(reflect_direc, 0)).xyz;

    vec3 light = ibl(
        world_normal, world_direc, albedo, F0, roughness, metallic
    );

    // Directional light
    {
        uint selected_dlight = 3;
        for (uint i = 0; i < 3; ++i) {
            if (u_comp_main.dlight_cascade_depths[i] > depth) {
                selected_dlight = i;
                break;
            }
        }

        const float lit = how_much_not_in_cascade_shadow(
            world_pos,
            CASCADE_OFFSETS[selected_dlight],
            u_comp_main.dlight_mats[selected_dlight],
            u_dlight_shadow_map
        );

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            u_comp_main.dlight_dir.xyz,
            u_comp_main.dlight_color.rgb
        ) * lit;
    }

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

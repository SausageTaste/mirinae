#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

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

layout(set = 0, binding = 5) uniform sampler2D u_dlight_shadow_map;
layout(set = 0, binding = 6) uniform sampler2D u_slight_shadow_map;
layout(set = 0, binding = 7) uniform samplerCube u_env_diffuse;
layout(set = 0, binding = 8) uniform samplerCube u_env_specular;
layout(set = 0, binding = 9) uniform sampler2D u_env_lut;


vec3 calc_frag_pos(float depth) {
    vec4 clip_pos = vec4(v_uv_coord * 2 - 1, depth, 1);
    vec4 frag_pos = u_comp_main.proj_inv * clip_pos;
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


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
            if (u_comp_main.dlight_cascade_depths[i] > depth_texel) {
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

    // Point light
    const vec3[] plight_poses = vec3[](
        vec3(23.063730118685, 7.4065945685438, -40.161457844117),
        vec3(-12.59026524203, 3.7424761322639, -57.913845097703),
        vec3(-27.187442727148, 2.8160032999329, -59.140437604074),
        vec3(-11.314874664971, 4.1150336202454, -72.997129503343),
        vec3(406.27578499388, 26.460298310523, -211.20135855825),
        vec3(17.723524030452, 4.5189922397826, -70.44753798105),
        vec3(-3.6262097794371, 0.75823541896961, -0.12247724517385)
    );
    const vec3[] plight_colors = vec3[](
        vec3(7, 24, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(240, 180, 70),
        vec3(7, 18, 24) * 2,
        vec3(1, 1, 1) * 5
    );

    for (uint i = 0; i < plight_poses.length(); ++i) {
        const vec3 light_pos = (u_comp_main.view * vec4(plight_poses[i], 1)).xyz;
        const vec3 cam_to_light = light_pos;
        const float projected_light_distance = dot(cam_to_light, view_direc);
        const vec3 projected_light_pos = projected_light_distance * view_direc;

        const float h = distance(light_pos, projected_light_pos);
        const float a = dot(-projected_light_pos, view_direc);
        const float b = dot(frag_pos - projected_light_pos, view_direc);
        const float c = (atan(b / h) / h) - (atan(a / h) / h);
        light += plight_colors[i] * c * 0.01;

        const vec3 to_light = light_pos - frag_pos;
        const vec3 to_light_n = normalize(to_light);
        const vec3 to_light_dir = cam_to_light;

        const float attenuation = 1.0 / dot(to_light, to_light);

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            to_light_n,
            plight_colors[i]
        ) * attenuation;
    }

    f_color = vec4(light, 1);
}

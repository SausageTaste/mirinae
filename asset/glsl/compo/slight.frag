#version 450

#include "../utils/lighting.glsl"

layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_depth_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;
layout(set = 0, binding = 3) uniform sampler2D u_material_map;

layout(set = 0, binding = 4) uniform U_CompoSlightMain {
    mat4 proj;
    mat4 proj_inv;
    mat4 view;
    mat4 view_inv;
    vec4 fog_color_density;
    float mie_anisotropy;
} u_main;

layout (set = 1, binding = 0) uniform sampler2DShadow u_shadow_map;

layout (push_constant) uniform U_CompoSlightPushConst {
    mat4 light_mat;
    vec4 pos_n_inner_angle;
    vec4 dir_n_outer_angle;
    vec4 color_n_max_dist;
} u_pc;


vec3 calc_frag_pos(float depth) {
    vec4 clip_pos = vec4(v_uv_coord * 2 - 1, depth, 1);
    vec4 frag_pos = u_main.proj_inv * clip_pos;
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


float calc_depth(vec3 frag_pos_v) {
    const vec4 clip_pos = u_main.proj * vec4(frag_pos_v, 1);
    return clip_pos.z / clip_pos.w;
}


vec3 make_shadow_texco(const vec3 frag_pos_v) {
    const vec4 frag_pos_in_dlight = u_pc.light_mat * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 sample_coord = proj_coords.xy * 0.5 + 0.5;
    return vec3(sample_coord, proj_coords.z);
}


void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos = calc_frag_pos(depth_texel);
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.y;
    const float metallic = material_texel.z;

    const vec3 world_pos = (u_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 world_normal = (u_main.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 world_direc = (u_main.view_inv * vec4(view_direc, 0)).xyz;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const vec3 reflect_direc = reflect(view_direc, normal);
    const vec3 world_reflect = (u_main.view_inv * vec4(reflect_direc, 0)).xyz;

    vec3 light = vec3(0);

    // Volumetric scattering
    {
        const int SAMPLE_COUNT = 5;
        const float INTENSITY = 0.6;

        const float light_factor = INTENSITY * phase_mie(dot(view_direc, u_pc.dir_n_outer_angle.xyz), u_main.mie_anisotropy) / float(SAMPLE_COUNT);
        const vec3 vec_step = frag_pos / float(-SAMPLE_COUNT - 1);
        const float dither_value = get_dither_value();

        for (int i = 0; i < SAMPLE_COUNT; ++i) {
            const float sample_factor = float(i + 0.5) * dither_value;
            const vec3 sample_pos = frag_pos + vec_step * sample_factor;
            const float sample_dist = length(sample_pos);
            const vec3 texco = make_shadow_texco(sample_pos);
            const float lit = texture(u_shadow_map, texco);

            const float attenuation = calc_slight_attenuation(
                sample_pos,
                u_pc.pos_n_inner_angle.xyz,
                -u_pc.dir_n_outer_angle.xyz,
                u_pc.pos_n_inner_angle.w,
                u_pc.dir_n_outer_angle.w
            ) * calc_attenuation(
                sample_dist, u_pc.color_n_max_dist.w
            );

            light += u_pc.color_n_max_dist.xyz * (light_factor * lit * attenuation);
        }
    }

    // Spotlight
    {
        const vec3 light_pos = u_pc.pos_n_inner_angle.xyz;
        const vec3 to_light = normalize(light_pos - frag_pos);
        const vec3 to_light_dir = u_pc.dir_n_outer_angle.xyz;

        const float attenuation = calc_slight_attenuation(
            frag_pos,
            light_pos,
            -to_light_dir,
            u_pc.pos_n_inner_angle.w,
            u_pc.dir_n_outer_angle.w
        ) * calc_attenuation(
            frag_distance, u_pc.color_n_max_dist.w
        );

        const vec3 texco = make_shadow_texco(frag_pos);
        const float lit = texture(u_shadow_map, texco);

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            to_light,
            u_pc.color_n_max_dist.xyz
        ) * attenuation * lit;
    }

    // Fog
    {
        const float x = frag_distance * u_main.fog_color_density.w;
        const float xx = x * x;
        const float fog_factor = 1.0 / exp(xx);
        light = mix(u_main.fog_color_density.xyz, light, fog_factor);
    }

    f_color = vec4(light, 1);
}

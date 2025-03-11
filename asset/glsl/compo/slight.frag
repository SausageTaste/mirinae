#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

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
} u_main;

layout (set = 1, binding = 0) uniform sampler2D u_shadow_map;

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

    const vec3 world_pos = (u_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 world_normal = (u_main.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 world_direc = (u_main.view_inv * vec4(view_direc, 0)).xyz;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const vec3 reflect_direc = reflect(view_direc, normal);
    const vec3 world_reflect = (u_main.view_inv * vec4(reflect_direc, 0)).xyz;

    vec3 light = vec3(0);

    // Flashlight
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

        const float not_shadow = how_much_not_in_shadow_pcf_bilinear(
            world_pos, u_pc.light_mat, u_shadow_map
        );

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            to_light,
            u_pc.color_n_max_dist.xyz
        ) * attenuation * not_shadow;
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

#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

layout (location = 0) in vec2 v_uv_coord;

layout (location = 0) out vec4 f_color;


layout (set = 0, binding = 0) uniform sampler2D u_depth_map;
layout (set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 0, binding = 2) uniform sampler2D u_normal_map;
layout (set = 0, binding = 3) uniform sampler2D u_material_map;

layout (set = 0, binding = 4) uniform U_CompoDlightMain {
    mat4 proj;
    mat4 proj_inv;
    mat4 view;
    mat4 view_inv;
    vec4 fog_color_density;
} u_main;

layout (set = 1, binding = 0) uniform sampler2D u_shadow_map;

layout (set = 1, binding = 1) uniform U_CompoDlightShadowMap {
    mat4 light_mats[4];
    vec4 cascade_depths;
    vec4 dlight_color;
    vec4 dlight_dir;
} ubuf_sh;


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

    // Directional light
    {
        uint selected_dlight = 3;
        for (uint i = 0; i < 3; ++i) {
            if (ubuf_sh.cascade_depths[i] > depth_texel) {
                selected_dlight = i;
                break;
            }
        }

        const float lit = how_much_not_in_cascade_shadow(
            world_pos,
            CASCADE_OFFSETS[selected_dlight],
            ubuf_sh.light_mats[selected_dlight],
            u_shadow_map
        );

        light += lit * calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            ubuf_sh.dlight_dir.xyz,
            ubuf_sh.dlight_color.rgb
        );
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

#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

layout (location = 0) in vec2 v_uv_coord;

layout (location = 0) out vec4 f_color;


layout (set = 0, binding = 0) uniform sampler2D u_depth_map;
layout (set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 0, binding = 2) uniform sampler2D u_normal_map;
layout (set = 0, binding = 3) uniform sampler2D u_material_map;
layout (set = 0, binding = 4) uniform sampler2D u_trans_lut;
layout (set = 0, binding = 5) uniform sampler2D u_multi_scat;
layout (set = 0, binding = 6) uniform sampler2D u_sky_view_lut;
layout (set = 0, binding = 7) uniform sampler2D u_cam_scat_vol;

layout (set = 0, binding = 8) uniform U_CompoAtmosSurfMain {
    mat4 proj;
    mat4 proj_inv;
    mat4 view;
    mat4 view_inv;
    vec4 fog_color_density;
    float mie_anisotropy;
} u_main;

layout (set = 1, binding = 0) uniform sampler2DShadow u_shadow_map;

layout (set = 1, binding = 1) uniform U_CompoDlightShadowMap {
    mat4 light_mats[4];
    vec4 cascade_depths;
    vec4 dlight_color;
    vec4 dlight_dir;
} ubuf_sh;


vec3 make_shadow_texco(const vec3 frag_pos_v, const uint selected_cascade) {
    const vec4 frag_pos_in_dlight = ubuf_sh.light_mats[selected_cascade] * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return vec3(texco, proj_coords.z);
}


void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos = calc_frag_pos(depth_texel, v_uv_coord, u_main.proj_inv);
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
        const float INTENSITY_DLIGHT = 0.6;

        const float dlight_factor = INTENSITY_DLIGHT * phase_mie(dot(view_direc, ubuf_sh.dlight_dir.xyz), u_main.mie_anisotropy) / float(SAMPLE_COUNT);
        const vec3 vec_step = frag_pos / float(-SAMPLE_COUNT - 1);
        const float dither_value = get_dither_value();

        for (int i = 0; i < SAMPLE_COUNT; ++i) {
            const float sample_factor = float(i + 0.5) * dither_value;
            const vec3 sample_pos = frag_pos + vec_step * sample_factor;
            const float sample_depth = calc_depth(sample_pos, u_main.proj);
            const uint selected_dlight = select_cascade(sample_depth, ubuf_sh.cascade_depths);
            const vec3 texco = make_shadow_texco(sample_pos, selected_dlight);
            const float lit = texture(u_shadow_map, texco);
            light += ubuf_sh.dlight_color.rgb * (dlight_factor * lit);
        }
    }

    // Directional light
    {
        const uint selected_dlight = select_cascade(depth_texel, ubuf_sh.cascade_depths);
        const vec3 texco = make_shadow_texco(frag_pos, selected_dlight);
        const float lit = texture(u_shadow_map, texco);

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
    f_color = vec4(1, 0.5, 0.25, 0);
}

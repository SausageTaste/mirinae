#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec3 v_frag_pos;
layout(location = 4) in vec2 v_texcoord;

layout(location = 0) out vec4 out_compo;


layout(set = 0, binding = 0) uniform U_TranspSkinnedFrame {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;

    // Directional light
    mat4 dlight_mats[4];
    vec4 dlight_dir;
    vec4 dlight_color;
    vec4 dlight_cascade_depths;

    float mie_anisotropy;
} u_main;

layout(set = 0, binding = 1) uniform sampler2DShadow u_dlight_shadow_maps[1];
layout(set = 0, binding = 2) uniform sampler2DShadow u_slight_shadow_maps[1];
layout(set = 0, binding = 3) uniform samplerCube u_env_diffuse;
layout(set = 0, binding = 4) uniform samplerCube u_env_specular;
layout(set = 0, binding = 5) uniform sampler2D u_env_lut;

layout(set = 1, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 1, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 1, binding = 2) uniform sampler2D u_normal_map;
layout(set = 1, binding = 3) uniform sampler2D u_orm_map;


float calc_depth(vec3 frag_pos_v) {
    const vec4 clip_pos = u_main.proj * vec4(frag_pos_v, 1);
    return clip_pos.z / clip_pos.w;
}


uint select_cascade(float depth) {
    for (uint i = 0; i < 3; ++i) {
        if (u_main.dlight_cascade_depths[i] < depth) {
            return i;
        }
    }

    return 3;
}


vec3 make_shadow_texco(const vec3 frag_pos_v, const uint selected_cascade) {
    const vec4 frag_pos_in_dlight = u_main.dlight_mats[selected_cascade] * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return vec3(texco, proj_coords.z);
}


vec3 ibl(
    const vec3 normal,
    const vec3 view_direc,
    const vec3 albedo,
    const vec3 f0,
    const float roughness,
    const float metallic,
    samplerCube env_diffuse,
    samplerCube env_specular
) {
    const vec3 N = normalize(normal);
    const vec3 V = -normalize(view_direc);
    const vec3 R = reflect(-V, N);
    const float NoV = max(dot(N, V), 0.0);
    const vec3 F = fresnel_schlick_rughness(NoV, f0, roughness);

    const vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const vec3 diffuse = texture(env_diffuse, N).rgb * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    const float mip_lvl = roughness * MAX_REFLECTION_LOD;
    const vec3 prefiltered_color = textureLod(env_specular, R, mip_lvl).rgb;
    const vec2 env_brdf = texture(u_env_lut, vec2(NoV, roughness)).rg;
    const vec3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);

    return kD * diffuse + specular;
}


void main() {
    const vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    const vec4 normal_texel = texture(u_normal_map, v_texcoord);

    const float depth = gl_FragCoord.z;
    const vec3 frag_pos = v_frag_pos;
    const vec3 albedo = albedo_texel.rgb;
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;

    vec3 normal = normalize(v_tbn * (normal_texel.xyz * 2 - 1));
    if (normal.z < 0)
        normal = -normal;

    const vec3 world_pos = (u_main.view_inv * vec4(frag_pos, 1)).xyz;
    const vec3 world_normal = (u_main.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 world_direc = (u_main.view_inv * vec4(view_direc, 0)).xyz;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const vec3 reflect_direc = reflect(view_direc, normal);
    const vec3 world_reflect = (u_main.view_inv * vec4(reflect_direc, 0)).xyz;

    vec3 light = ibl(
        world_normal,
        world_direc,
        albedo,
        F0,
        roughness,
        metallic,
        u_env_diffuse,
        u_env_specular
    );

    // Volumetric scattering
    {
        const int SAMPLE_COUNT = 5;
        const float INTENSITY_DLIGHT = 0.6;

        const float dlight_factor = INTENSITY_DLIGHT * phase_mie(dot(view_direc, u_main.dlight_dir.xyz), u_main.mie_anisotropy) / float(SAMPLE_COUNT);
        const vec3 vec_step = frag_pos / float(-SAMPLE_COUNT - 1);
        const float dither_value = get_dither_value();

        for (int i = 0; i < SAMPLE_COUNT; ++i) {
            const float sample_factor = float(i + 0.5) * dither_value;
            const vec3 sample_pos = frag_pos + vec_step * sample_factor;
            const float sample_depth = calc_depth(sample_pos);
            const uint selected_dlight = select_cascade(sample_depth);
            const vec3 texco = make_shadow_texco(sample_pos, selected_dlight);
            const float lit = texture(u_dlight_shadow_maps[0], texco);
            light += u_main.dlight_color.rgb * (dlight_factor * lit);
        }
    }

    // Directional light
    {
        const uint selected_dlight = select_cascade(depth);
        const vec3 texco = make_shadow_texco(frag_pos, selected_dlight);
        const float lit = texture(u_dlight_shadow_maps[0], texco);

        light += lit * calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            u_main.dlight_dir.xyz,
            u_main.dlight_color.rgb
        );
    }

    out_compo.rgb = light;
    out_compo.a = albedo_texel.a;
}

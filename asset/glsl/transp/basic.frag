#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"
#include "../atmos/data.glsl"
#include "../atmos/integrate.glsl"

layout (location = 0) in mat3 v_tbn;
layout (location = 3) in vec3 v_frag_pos;
layout (location = 4) in vec2 v_texcoord;

layout (location = 0) out vec4 out_compo;


layout (set = 0, binding = 0) uniform U_TranspSkinnedFrame {
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

layout (set = 0, binding = 1) uniform sampler2DShadow u_dlight_shadow_maps[1];
layout (set = 0, binding = 2) uniform sampler2DShadow u_slight_shadow_maps[1];
layout (set = 0, binding = 3) uniform samplerCube u_env_diffuse;
layout (set = 0, binding = 4) uniform samplerCube u_env_specular;
layout (set = 0, binding = 5) uniform sampler2D u_env_lut;
layout (set = 0, binding = 6) uniform sampler2D u_trans_lut;
layout (set = 0, binding = 7) uniform sampler3D u_cam_scat_vol;

layout (set = 1, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout (set = 1, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 1, binding = 2) uniform sampler2D u_normal_map;
layout (set = 1, binding = 3) uniform sampler2D u_orm_map;


vec3 make_shadow_texco(const vec3 frag_pos_v, const uint selected_cascade) {
    const vec4 frag_pos_in_dlight = u_main.dlight_mats[selected_cascade] * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return vec3(texco, proj_coords.z);
}


vec3 ibl(
    const vec3 normal_v,
    const vec3 view_dir_v,
    const vec3 albedo,
    const vec3 f0,
    const float roughness,
    const float metallic,
    samplerCube env_diffuse,
    samplerCube env_specular
) {
    const vec3 N = normalize(normal_v);
    const vec3 V = -normalize(view_dir_v);
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


vec3 get_transmittance(vec3 frag_pos_w) {
    const AtmosphereParameters atmos_params = GetAtmosphereParameters();
    const float planet_radius = atmos_params.BottomRadius * 1000;
    const vec3 frag_pos_e = frag_pos_w + vec3(0, planet_radius, 0);
    const float frag_height_e = length(frag_pos_e);
    const vec3 frag_up_dir_e = normalize(frag_pos_e);
    const vec3 dlight_dir_w = normalize(mat3(u_main.view_inv) * u_main.dlight_dir.xyz);
    const float view_zenith_cos_angle = dot(dlight_dir_w, frag_up_dir_e);
    const vec2 lut_trans_uv = LutTransmittanceParamsToUv(atmos_params, frag_height_e / 1000.0, view_zenith_cos_angle);
    return textureLod(u_trans_lut, lut_trans_uv, 0).xyz;
}


void main() {
    const vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    const vec4 normal_texel = texture(u_normal_map, v_texcoord);

    const float depth = gl_FragCoord.z;
    const vec3 frag_pos_v = v_frag_pos;
    const vec3 albedo = albedo_texel.rgb;
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 normal_v = normalize(v_tbn * (normal_texel.xyz * 2 - 1));
    if (dot(normal_v, normalize(-frag_pos_v)) < 0)
        normal_v = -normal_v;

    const vec3 frag_pos_w = (u_main.view_inv * vec4(frag_pos_v, 1)).xyz;
    const vec3 normal_w = (u_main.view_inv * vec4(normal_v, 0)).xyz;
    const vec3 view_pos_w = u_main.view_inv[3].xyz;
    const vec3 view_dir_v = normalize(frag_pos_v);
    const vec3 view_dir_w = (u_main.view_inv * vec4(view_dir_v, 0)).xyz;
    const float frag_distance = length(frag_pos_v);

    // Aerial perspective
    {
        const float t_depth = length(frag_pos_w - view_pos_w);
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);	// squared distribution
        const vec4 cam_scat_texel = textureLod(u_cam_scat_vol, vec3(v_texcoord, w), 0);
        out_compo = weight * cam_scat_texel;
    }

    // IBL
    {
        out_compo.xyz += ibl(
            normal_w,
            view_dir_w,
            albedo,
            F0,
            roughness,
            metallic,
            u_env_diffuse,
            u_env_specular
        );
    }

    // Directional light
    {
        const vec3 transmittance = get_transmittance(frag_pos_w);
        const uint selected_dlight = select_cascade(depth, u_main.dlight_cascade_depths);

        float lit = 1;
        const vec3 texco = make_shadow_texco(frag_pos_v, selected_dlight);
        if (texco.x < 0 || texco.x > 1 || texco.y < 0 || texco.y > 1)
            lit = 1;
        else if (check_shadow_texco_range(texco))
            lit = texture(u_dlight_shadow_maps[0], texco);

        out_compo.xyz += lit * transmittance * calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal_v,
            F0,
            -view_dir_v,
            u_main.dlight_dir.xyz,
            vec3(1)
        );
    }

    out_compo.a = albedo_texel.a;
}

#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"
#include "../atmos/data.glsl"
#include "../atmos/integrate.glsl"

layout (location = 0) in vec2 v_uv_coord;

layout (location = 0) out vec4 f_color;


layout (set = 0, binding = 0) uniform sampler2D u_depth_map;
layout (set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 0, binding = 2) uniform sampler2D u_normal_map;
layout (set = 0, binding = 3) uniform sampler2D u_material_map;
layout (set = 0, binding = 4) uniform sampler2D u_trans_lut;
layout (set = 0, binding = 5) uniform sampler2D u_multi_scat;
layout (set = 0, binding = 6) uniform sampler2D u_sky_view_lut;
layout (set = 0, binding = 7) uniform sampler3D u_cam_scat_vol;

layout (set = 0, binding = 8) uniform U_CompoAtmosSurfMain {
    mat4 proj;
    mat4 proj_inv;
    mat4 view;
    mat4 view_inv;
    vec4 view_pos_w;
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


const float AP_SLICE_COUNT = 32;
const float AP_SLICE_COUNT_RCP = 1.0 / AP_SLICE_COUNT;


vec3 make_shadow_texco(const vec3 frag_pos_v, const uint selected_cascade) {
    const vec4 frag_pos_in_dlight = ubuf_sh.light_mats[selected_cascade] * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return vec3(texco, proj_coords.z);
}


bool check_shadow_texco_range(const vec3 texco) {
    if (texco.z < 0)
        return false;
    else if (texco.z > 1)
        return false;
    else if (texco.x < 0 || texco.x > 1 || texco.y < 0 || texco.y > 1)
        return false;
    else
        return true;

}


float AerialPerspectiveDepthToSlice(float depth) {
    const float AP_KM_PER_SLICE = 4;
    const float M_PER_SLICE_RCP = 1.0 / (AP_KM_PER_SLICE * 1000.0);
    return depth * M_PER_SLICE_RCP;
}


vec3 get_transmittance(vec3 frag_pos_w) {
    const AtmosphereParameters atmos_params = GetAtmosphereParameters();
    const float planet_radius = atmos_params.BottomRadius * 1000;
    const vec3 frag_pos_e = frag_pos_w + vec3(0, planet_radius, 0);
    const float frag_height_e = length(frag_pos_e);
    const vec3 frag_up_dir_e = normalize(frag_pos_e);
    const vec3 dlight_dir_w = normalize(mat3(u_main.view_inv) * ubuf_sh.dlight_dir.xyz);
    const float view_zenith_cos_angle = dot(dlight_dir_w, frag_up_dir_e);
    const vec2 lut_trans_uv = LutTransmittanceParamsToUv(atmos_params, frag_height_e / 1000.0, view_zenith_cos_angle);
    return textureLod(u_trans_lut, lut_trans_uv, 0).xyz;
}


void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos_v = calc_frag_pos(depth_texel, v_uv_coord, u_main.proj_inv);
    const vec3 frag_pos_w = (u_main.view_inv * vec4(frag_pos_v, 1)).xyz;

    // Aerial perspective
    {
        const float t_depth = length(frag_pos_w - u_main.view_pos_w.xyz);
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);	// squared distribution
        const vec4 cam_scat_texel = textureLod(u_cam_scat_vol, vec3(v_uv_coord, w), 0);
        f_color = weight * cam_scat_texel;
    }

    // Directional light
    {
        const vec3 albedo = albedo_texel.rgb;
        const vec3 normal_v = normalize(normal_texel.xyz * 2 - 1);
        const float roughness = material_texel.y;
        const float metallic = material_texel.z;
        const vec3 F0 = mix(vec3(0.04), albedo, metallic);
        const vec3 view_dir_v = normalize(frag_pos_v);

        const vec3 transmittance = get_transmittance(frag_pos_w);
        const uint selected_dlight = select_cascade(depth_texel, ubuf_sh.cascade_depths);

        float lit = 1;
        const vec3 texco = make_shadow_texco(frag_pos_v, selected_dlight);
        if (texco.x < 0 || texco.x > 1 || texco.y < 0 || texco.y > 1)
            lit = 1;
        else if (check_shadow_texco_range(texco))
            lit = texture(u_shadow_map, texco);

        f_color.xyz += lit * transmittance * calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal_v,
            F0,
            -view_dir_v,
            ubuf_sh.dlight_dir.xyz,
            vec3(1)
        );
    }

}

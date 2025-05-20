#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"
#include "../atmos/data.glsl"

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


vec3 make_shadow_texco(const vec3 frag_pos_v, const uint selected_cascade) {
    const vec4 frag_pos_in_dlight = ubuf_sh.light_mats[selected_cascade] * vec4(frag_pos_v, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return vec3(texco, proj_coords.z);
}


const float AP_SLICE_COUNT = 32;
const float AP_SLICE_COUNT_RCP = 1.0 / AP_SLICE_COUNT;

float AerialPerspectiveDepthToSlice(float depth) {
    const float AP_KM_PER_SLICE = 4;
    const float M_PER_SLICE_RCP = 1.0 / (AP_KM_PER_SLICE * 1000.0);
    return depth * M_PER_SLICE_RCP;
}

void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos_v = calc_frag_pos(depth_texel, v_uv_coord, u_main.proj_inv);
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal_v = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.y;
    const float metallic = material_texel.z;

    const vec3 frag_pos_w = (u_main.view_inv * vec4(frag_pos_v, 1)).xyz;
    const vec3 normal_w = mat3(u_main.view_inv) * normal_v;
    const vec3 view_dir_v = normalize(frag_pos_v);
    const vec3 view_dir_w = mat3(u_main.view_inv) * view_dir_v;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    const AtmosphereParameters atmos_params = GetAtmosphereParameters();

    const vec3 cam_pos_e = u_main.view_pos_w.xyz + vec3(0, atmos_params.BottomRadius, 0);
    const float cam_height_e = length(cam_pos_e);

    const float tDepth = length(frag_pos_w - u_main.view_pos_w.xyz);
    float slice = AerialPerspectiveDepthToSlice(tDepth);
    float weight = 1.0;
    if (slice < 0.5) {
        // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
        weight = clamp(slice * 2.00, 0, 1);
        slice = 0.5;
    }
    const float w = sqrt(slice * AP_SLICE_COUNT_RCP);	// squared distribution
    f_color = weight * textureLod(u_cam_scat_vol, vec3(v_uv_coord, w), 0);
}

#version 450

#include "../utils/complex.glsl"
#include "../utils/lighting.glsl"
#include "../utils/normal_mapping.glsl"

layout (location = 0) in vec4 i_lod_scales;
layout (location = 1) in vec4 i_frag_pos_scrn;
layout (location = 2) in vec3 i_frag_pos_v;
layout (location = 3) in vec2 i_uv;

layout (location = 0) out vec4 f_color;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 patch_offset_scale;
    vec4 tile_dims_n_fbuf_size;
    vec4 tile_index_count;
    float patch_height;
} u_pc;

layout (set = 0, binding = 0) uniform U_OceanTessParams {
    mat4 light_mats[4];
    vec4 texco_offset_rot_[3];
    vec4 dlight_cascade_depths;
    vec4 dlight_color;
    vec4 dlight_dir;
    vec4 fog_color_density;
    vec4 jacobian_scale;
    vec4 len_lod_scales;
    vec4 ocean_color;
    float foam_bias;
    float foam_scale;
    float foam_threshold;
    float roughness;
    float sss_base;
    float sss_scale;
} u_params;

layout (set = 0, binding = 1) uniform sampler2D u_disp_map[3];
layout (set = 0, binding = 2) uniform sampler2D u_deri_map[3];
layout (set = 0, binding = 3) uniform sampler2D u_turb_map[3];
layout (set = 0, binding = 4) uniform sampler2D u_trans_lut;
layout (set = 0, binding = 5) uniform sampler2D u_sky_view_lut;
layout (set = 0, binding = 6) uniform sampler3D u_cam_scat_vol;
layout (set = 0, binding = 7) uniform sampler2D u_sky_tex;
layout (set = 0, binding = 8) uniform sampler2DArrayShadow u_shadow_map;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 map_cube(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    uv = clamp(uv, 0.0, 1.0);
    return uv;
}


float meanFresnel(float cosThetaV, float sigmaV) {
    return pow(1.0 - cosThetaV, 5.0 * exp(-2.69 * sigmaV)) / (1.0 + 22.7 * pow(sigmaV, 1.5));
}


float meanFresnel(vec3 V, vec3 N, float sigmaSq) {
    return meanFresnel(dot(V, N), sqrt(sigmaSq));
}


float reflectedSunRadiance(vec3 V, vec3 N, vec3 L, float sigmaSq) {
    vec3 H = normalize(L + V);

    float hn = dot(H, N);
    float p = exp(-2.0 * ((1.0 - hn * hn) / sigmaSq) / (1.0 + hn)) / (4.0 * PI * sigmaSq);

    float c = 1.0 - dot(V, H);
    float c2 = c * c;
    float fresnel = 0.02 + 0.98 * c2 * c2 * c;

    float zL = dot(L, N);
    float zV = dot(V, N);
    zL = max(zL, 0.01);
    zV = max(zV, 0.01);

    // brdf times cos(thetaL)
    return zL <= 0.0 ? 0.0 : max(fresnel * p * sqrt(abs(zL / zV)), 0.0);
}


vec3 oceanRadiance(vec3 V, vec3 N, vec3 L, float seaRoughness, vec3 sunL, vec3 skyE, vec3 seaColor) {
    float F = meanFresnel(V, N, seaRoughness);
    F = pow(F, 1.0 / 5.0);
    vec3 Lsun = reflectedSunRadiance(V, N, L, seaRoughness) * sunL;
    vec3 Lsky = skyE * F / PI;
    vec3 Lsea = (1.0 - F) * seaColor * skyE / PI;
    return Lsun + Lsky + Lsea;
}


const float PLANET_BOTTOM = 6360;
const float PLANET_TOP = 6460;
const float AP_SLICE_COUNT = 32;
const float AP_SLICE_COUNT_RCP = 1.0 / AP_SLICE_COUNT;

float AerialPerspectiveDepthToSlice(float depth) {
    const float AP_KM_PER_SLICE = 4;
    const float M_PER_SLICE_RCP = 1.0 / (AP_KM_PER_SLICE * 1000.0);
    return depth * M_PER_SLICE_RCP;
}

float fromUnitToSubUvs(float u, float resolution) {
    return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}

vec2 SkyViewLutParamsToUv(
    bool IntersectGround,
    float viewZenithCosAngle,
    float lightViewCosAngle,
    float viewHeight
) {
    vec2 uv;

    const float Vhorizon = sqrt(
        viewHeight * viewHeight - PLANET_BOTTOM * PLANET_BOTTOM
    );
    const float CosBeta = Vhorizon / viewHeight;  // GroundToHorizonCos
    const float Beta = acos(CosBeta);
    const float ZenithHorizonAngle = PI - Beta;

    if (!IntersectGround) {
        float coord = acos(viewZenithCosAngle) / ZenithHorizonAngle;
        coord = 1.0 - coord;
        coord = sqrt(coord);
        coord = 1.0 - coord;
        uv.y = coord * 0.5, 0, 0.5;
    } else {
        float coord = (acos(viewZenithCosAngle) - ZenithHorizonAngle) / Beta;
        coord = sqrt(coord);
        uv.y = coord * 0.5 + 0.5;
    }

    {
        float coord = -lightViewCosAngle * 0.5 + 0.5;
        coord = sqrt(coord);
        uv.x = coord;
    }

    // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage
    // visible)
    uv = vec2(fromUnitToSubUvs(uv.x, 192), fromUnitToSubUvs(uv.y, 108));
    return uv;
}

float raySphereIntersectNearest(vec3 r0, vec3 rd, vec3 s0, float sR) {
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sR * sR);
    float delta = b * b - 4.0 * a * c;
    if (delta < 0.0 || a == 0.0) {
        return -1.0;
    }
    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);
    if (sol0 < 0.0 && sol1 < 0.0) {
        return -1.0;
    }
    if (sol0 < 0.0) {
        return max(0.0, sol1);
    } else if (sol1 < 0.0) {
        return max(0.0, sol0);
    }
    return max(0.0, min(sol0, sol1));
}


vec2 LutTransmittanceParamsToUv(float viewHeight, float viewZenithCosAngle) {
    float H = sqrt(
        max(0.0, PLANET_TOP * PLANET_TOP - PLANET_BOTTOM * PLANET_BOTTOM)
    );
    float rho = sqrt(
        max(0.0, viewHeight * viewHeight - PLANET_BOTTOM * PLANET_BOTTOM)
    );

    float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) +
                         PLANET_TOP * PLANET_TOP;
    float d = max(
        0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))
    );  // Distance to atmosphere boundary

    float d_min = PLANET_TOP - viewHeight;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;

    return vec2(x_mu, x_r);
}


vec3 get_transmittance(vec3 frag_pos_w, vec3 dlight_dir_w) {
    const vec3 frag_pos_e = frag_pos_w + vec3(0, PLANET_BOTTOM * 1000, 0);
    const float frag_height_e = length(frag_pos_e);
    const vec3 frag_up_dir_e = normalize(frag_pos_e);
    const float view_zenith_cos_angle = dot(dlight_dir_w, frag_up_dir_e);
    const vec2 lut_trans_uv = LutTransmittanceParamsToUv(frag_height_e / 1000, view_zenith_cos_angle);
    return textureLod(u_trans_lut, lut_trans_uv, 0).xyz;
}


vec3 make_shadow_texco(vec3 frag_pos_v, mat4 light_mat) {
    vec4 frag_pos_in_dlight = light_mat * vec4(frag_pos_v, 1);
    vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    vec2 sample_coord = proj_coords.xy * 0.5 + 0.5;
    return vec3(sample_coord, max(proj_coords.z, 0));
}


uint select_cascade(float depth) {
    for (uint i = 0; i < 3; ++i) {
        if (u_params.dlight_cascade_depths[i] < depth) {
            return i;
        }
    }

    return 3;
}


void main() {
    const mat4 view_inv = inverse(u_pc.view);
    const mat3 view_inv3 = mat3(view_inv);
    const mat3 tbn = make_tbn_mat(vec3(0, 1, 0), vec3(1, 0, 0), u_pc.view * u_pc.model);
    const vec3 albedo = u_params.ocean_color.xyz;
    const float roughness = u_params.roughness;
    const float metallic = 0;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec4 derivatives = vec4(0);
    {
        derivatives += texture(u_deri_map[0], i_uv / u_params.len_lod_scales[0]);
        derivatives += texture(u_deri_map[1], i_uv / u_params.len_lod_scales[1]) * i_lod_scales.y;
        derivatives += texture(u_deri_map[2], i_uv / u_params.len_lod_scales[2]) * i_lod_scales.z;
    }

    const vec2 slope = vec2(
        derivatives.x / (1.0 + derivatives.z),
        derivatives.y / (1.0 + derivatives.w)
    );
    const vec3 normal_m = normalize(vec3(-slope.x, 1, -slope.y));
    const vec3 normal_w = normalize(mat3(u_pc.model) * normal_m);
    const vec3 normal_v = normalize(mat3(u_pc.view) * normal_w);
    const vec3 frag_pos_w = (view_inv * vec4(i_frag_pos_v, 1)).xyz;
    const vec3 to_sun_dir_v = normalize(u_params.dlight_dir.xyz);
    const vec3 to_sun_dir_w = normalize(view_inv3 * to_sun_dir_v);
    const float frag_dist = length(i_frag_pos_v);
    const vec3 cam_dir_v = normalize(i_frag_pos_v);
    const vec3 cam_dir_w = normalize(view_inv3 * cam_dir_v);
    const vec3 cam_pos_w = view_inv[3].xyz;
    const vec3 sun_trans = get_transmittance(frag_pos_w, to_sun_dir_w);

    vec3 light = vec3(0);

    float lit = 1;
    {
        const uint selected_dlight = select_cascade(gl_FragCoord.z);
        const vec3 texco = make_shadow_texco(i_frag_pos_v, u_params.light_mats[selected_dlight]);
        lit = texture(u_shadow_map, vec4(texco.xy, selected_dlight, texco.z));
    }

    // Sky
    vec3 sky_color = vec3(0);
    {
        vec3 cam_dir_reflec_w = reflect(cam_dir_w, normal_w);
        if (cam_dir_reflec_w.y < 0) {
            cam_dir_reflec_w.y = 0;
            cam_dir_reflec_w = normalize(cam_dir_reflec_w);
        }

        const float planet_radius = PLANET_BOTTOM * 1000;
        const vec3 cam_pos_e = cam_pos_w + vec3(0, planet_radius, 0);
        const float cam_height_e = length(cam_pos_e);

        const vec3 up_dir_e = normalize(cam_pos_e);
        const float view_zenith_cos_angle = dot(cam_dir_reflec_w, up_dir_e);

        // assumes non parallel vectors
        const vec3 side_dir_e = normalize(cross(up_dir_e, cam_dir_w));
        // aligns toward the sun light but perpendicular to up vector
        const vec3 forward_dir_e = normalize(cross(side_dir_e, up_dir_e));
        const vec2 light_on_plane = normalize(
            vec2(dot(to_sun_dir_w, forward_dir_e), dot(to_sun_dir_w, side_dir_e))
        );
        const float light_view_cos_angle = light_on_plane.x;
        const bool intersect_ground = raySphereIntersectNearest(cam_pos_e, cam_dir_reflec_w, vec3(0), planet_radius) >= 0;

        const vec2 uv = SkyViewLutParamsToUv(
            intersect_ground,
            view_zenith_cos_angle,
            light_view_cos_angle,
            cam_height_e / 1000
        );

        const vec4 sky_view_texel = textureLod(u_sky_view_lut, uv, 0);
        sky_color = sky_view_texel.xyz;
    }

    {
        const vec3 to_cam_w = -cam_dir_w;
        light += oceanRadiance(
            to_cam_w,
            normal_m,
            to_sun_dir_w,
            roughness,
            sun_trans * 10 * lit,
            sky_color,
            albedo
        );
    }

    // Foam
    /*
    {
        float jacobian =
              texture(u_turb_map[0], i_uv / u_params.len_lod_scales[0]).x * u_params.jacobian_scale[0]
            + texture(u_turb_map[1], i_uv / u_params.len_lod_scales[1]).x * u_params.jacobian_scale[1]
            + texture(u_turb_map[2], i_uv / u_params.len_lod_scales[2]).x * u_params.jacobian_scale[2];

        jacobian = (-jacobian + u_params.foam_bias) * u_params.foam_scale;
        jacobian = clamp(jacobian, 0, 1);
        jacobian *= clamp(u_params.len_lod_scales[3] / frag_dist, 0, 1);

        const vec3 foam_light = calc_pbr_illumination(
            0.5,
            metallic,
            vec3(1),
            normal_v,
            F0,
            -cam_dir_v,
            to_sun_dir_v,
            u_params.dlight_color.xyz
        );

        light = mix(light, foam_light, jacobian);
    }
    */

    // Aerial perspective
    {
        const float t_depth = frag_dist;
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const vec2 texco = i_frag_pos_scrn.xy * (0.5 / i_frag_pos_scrn.w) + 0.5;
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);  // squared distribution
        const vec4 cam_scat_texel = textureLod(u_cam_scat_vol, vec3(texco, w), 0);
        light += cam_scat_texel.xyz * weight;
    }

    f_color.xyz = light;
    f_color.w = 1;
}

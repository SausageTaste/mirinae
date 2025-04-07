#version 450

#include "../utils/complex.glsl"
#include "../utils/lighting.glsl"
#include "../utils/normal_mapping.glsl"

layout (location = 0) in vec4 i_lod_scales;
layout (location = 1) in vec3 i_frag_pos;
layout (location = 2) in vec2 i_uv;

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
    vec4 texco_offset_rot_[3];
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
layout (set = 0, binding = 4) uniform sampler2D u_sky_tex;


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


void main() {
    const mat4 view_inv = inverse(u_pc.view);
    const mat3 view_inv3 = mat3(view_inv);
    const mat3 tbn = make_tbn_mat(vec3(0, 1, 0), vec3(1, 0, 0), u_pc.view * u_pc.model);
    const vec3 albedo = u_params.ocean_color.xyz;
    const float roughness = u_params.roughness;
    const float metallic = 0;
    const float frag_dist = length(i_frag_pos);
    const vec3 to_view = i_frag_pos / (-frag_dist);
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
    const vec3 world_normal = normalize(vec3(-slope.x, 1, -slope.y));
    const vec3 world_view = normalize(view_inv3 * (-i_frag_pos));
    const vec3 normal = normalize(mat3(u_pc.view * u_pc.model) * world_normal);
    const vec3 light_dir = view_inv3 * (u_params.dlight_dir.xyz);

    vec3 light = vec3(0);

    vec3 l_world = reflect(-world_view, world_normal);
    vec2 texco = map_cube(l_world);
    texco.y = clamp(texco.y, 0.0, 0.5);
    vec3 refl = textureLod(u_sky_tex, texco, 0).xyz;
    vec3 surfaceColor = oceanRadiance(world_view, world_normal, light_dir, roughness, u_params.dlight_color.xyz, refl, albedo);
    light += surfaceColor;

    // Foam
    {
        float jacobian =
              texture(u_turb_map[0], i_uv / u_params.len_lod_scales[0]).x * u_params.jacobian_scale[0]
            + texture(u_turb_map[1], i_uv / u_params.len_lod_scales[1]).x * u_params.jacobian_scale[1]
            + texture(u_turb_map[2], i_uv / u_params.len_lod_scales[2]).x * u_params.jacobian_scale[2];

        jacobian = (-jacobian + u_params.foam_bias) * u_params.foam_scale;
        jacobian = clamp(jacobian, 0, 1);
        jacobian *= clamp(u_params.len_lod_scales[3] / frag_dist, 0, 1);

        vec3 foam_light = calc_pbr_illumination(
            0.5,
            metallic,
            vec3(1),
            normal,
            F0,
            -normalize(i_frag_pos),
            light_dir,
            u_params.dlight_color.xyz
        );

        light = mix(light, foam_light, jacobian);
    }

    // Fog
    {
        const float x = frag_dist * u_params.fog_color_density.w;
        const float xx = x * x;
        const float fog_factor = 1.0 / exp(xx);
        light = mix(u_params.fog_color_density.xyz, light, fog_factor);
    }

    f_color.xyz = light;
    f_color.w = 1;
}

#version 450

#include "../utils/complex.glsl"
#include "../utils/lighting.glsl"
#include "../utils/normal_mapping.glsl"

layout (location = 0) in vec3 i_frag_pos;
layout (location = 1) in vec2 i_uv;

layout (location = 0) out vec4 f_color;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    float foam_threshold;
    float foam_bias;
} u_pc;

layout (set = 0, binding = 0) uniform U_OceanTessParams {
    vec4 fog_color_density;
    vec4 texco_offset_rot_[3];
    vec4 height_map_size_fbuf_size;
    vec2 tile_dimensions;
} u_params;

layout (set = 0, binding = 1) uniform sampler2D u_height_map[3];
layout (set = 0, binding = 2) uniform sampler2D u_normal_map[3];
layout (set = 0, binding = 3) uniform sampler2D u_sky_tex;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 map_cube(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


vec2 transform_uv(vec2 uv, int cascade) {
    vec2 tile_idx = u_pc.tile_index_count.xy;
    vec2 offset = u_params.texco_offset_rot_[cascade].xy;
    vec2 scale = u_params.texco_offset_rot_[cascade].zw;

    const vec2 global_uv = complex_init(uv + tile_idx);
    const vec2 offset_rot = complex_init(scale);
    return complex_mul(global_uv, offset_rot) + offset;
}


vec3 merge_normals() {
    vec3 normal = vec3(0);

    for (int i = 0; i < 3; i++) {
        vec3 texel = textureLod(u_normal_map[i], transform_uv(i_uv, i), 0).xyz;
        normal += (texel * 2 - 1);
    }

    return normal;
}


void main() {
    const mat3 tbn = make_tbn_mat(vec3(0, 1, 0), vec3(1, 0, 0), u_pc.view * u_pc.model);
    const vec3 normal = normalize(tbn * merge_normals());
    const vec3 albedo = vec3(0.1, 0.15, 0.25);
    const float roughness = 0.1;
    const float metallic = 0;
    const float frag_dist = length(i_frag_pos);
    const vec3 to_view = i_frag_pos / (-frag_dist);

    vec3 light = vec3(0);

    const vec3 light_dir = mat3(u_pc.view) *  normalize(vec3(0.5653, 0.3, 0.3812));

    /*
    light += calc_pbr_illumination(
        0.2,
        0,
        albedo,
        normal,
        F0,
        -normalize(i_frag_pos),
        light_dir,
        vec3(3)
    );
    */

    {
        vec3 n = normal;
        vec3 v = to_view;
        vec3 l = reflect(-v, n);

        // Fresnel faktor (levegő IOR: 1.000293f, víz IOR: 1.33f)
        float F0 = 0.020018673;
        float F = F0 + (1.0 - F0) * pow(1.0 - dot(n, l), 5.0);

        // tükröződő égbolt
        vec3 l_world = (inverse(u_pc.view) * vec4(l, 0)).xyz;
        vec3 refl = textureLod(u_sky_tex, map_cube(l_world), 0).xyz;

        // habzás (az ARM/Mali példakódja alapján)
        float turbulence = max(1.6 - 3, 0.0);
        float color_mod = 1.0 + 3.0 * smoothstep(1.2, 1.8, turbulence);

        // napfény (Ward modell)
        const vec3 sundir = light_dir;
        const float rho   = 0.3;
        const float ax    = 0.25;
        const float ay    = 0.1;

        vec3 h = sundir + v;
        vec3 x = cross(sundir, n);
        vec3 y = cross(x, n);

        const float ONE_OVER_4PI = 1.0 / (4.0 * PI);
        float mult = (ONE_OVER_4PI * rho / (ax * ay * sqrt(max(1e-5, dot(sundir, n) * dot(v, n)))));
        float hdotx = dot(h, x) / ax;
        float hdoty = dot(h, y) / ay;
        float hdotn = dot(h, n);
        float spec = mult * exp(-((hdotx * hdotx) + (hdoty * hdoty)) / (hdotn * hdotn));

        vec3 oceanColor = albedo;
        light += vec3(mix(oceanColor, refl * color_mod, F) + vec3(1) * spec);
    }

    // Foam
    {
        float foam = 0;
        for (int i = 0; i < 3; i++) {
            vec2 uv = transform_uv(i_uv, i);
            vec3 D = textureOffset(u_height_map[i], uv, ivec2(0, 0)).xyz;
            vec3 Dx = textureOffset(u_height_map[i], uv, ivec2(1, 0)).xyz;
            vec3 Dz = textureOffset(u_height_map[i], uv, ivec2(0, 1)).xyz;
            vec2 dDx = (Dx.xz - D.xz) * 256 / 20;
            vec2 dDz = (Dz.xz - D.xz) * 256 / 20;
            float J = (1.0 + dDx.x) * (1.0 + dDz.y) - dDx.y * dDz.x;
            const float biasedJacobian = max(0.0, -(J - u_pc.foam_bias));
            if (biasedJacobian > u_pc.foam_threshold)
                foam = max(foam, biasedJacobian);
        }
        foam = clamp(foam, 0, 1);
        light = mix(light, vec3(1), foam);
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

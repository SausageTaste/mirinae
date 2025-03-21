#include "konst.glsl"


float distribution_ggx(const vec3 N, const vec3 H, const float roughness) {
    const float a = roughness*roughness;
    const float a2 = a*a;
    const float NdotH = max(dot(N, H), 0.0);
    const float NdotH2 = NdotH*NdotH;

    const float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.00001);
}


float geometry_schlick_ggx(const float NdotV, const float roughness) {
    const float r = (roughness + 1.0);
    const float k = (r*r) / 8.0;

    const float nom   = NdotV;
    const float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


float geometry_smith(
    const vec3 N,
    const vec3 V,
    const vec3 L,
    const float roughness
) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    const float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    const float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}


float geometry_schlick_ggx_ibl(const float NdotV, const float roughness) {
    const float a = roughness;
    const float k = (a * a) / 2.0;

    const float nom   = NdotV;
    const float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


float geometry_smith_ibl(
    const vec3 N,
    const vec3 V,
    const vec3 L,
    const float roughness
) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    const float ggx2 = geometry_schlick_ggx_ibl(NdotV, roughness);
    const float ggx1 = geometry_schlick_ggx_ibl(NdotL, roughness);

    return ggx1 * ggx2;
}


vec3 fresnel_schlick(const float cosTheta, const vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}


vec3 fresnel_schlick_rughness(float cosTheta, vec3 F0, float roughness) {
    return (
        F0
        + (max(vec3(1.0 - roughness), F0) - F0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0)
    );
}


// https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
float calc_attenuation(const float frag_distance, const float max_light_dist) {
    const float s = frag_distance / max_light_dist;

    if (s > 1.0)
        return 0.0;

    const float ss = s * s;
    return (1.0 - ss) * (1.0 - ss) / (1.0 + ss * 1.0);
}


float calc_slight_attenuation(
    const vec3 frag_pos,
    const vec3 light_pos,
    const vec3 light_direc,
    const float fade_start,
    const float fade_end
) {
    const vec3 light_to_frag_n = normalize(frag_pos - light_pos);
    const float theta = dot(light_to_frag_n, light_direc);
    const float epsilon = fade_start - fade_end;
    const float atten_factor = 1;

    return clamp((theta - fade_end) / epsilon * atten_factor, 0.0, 1.0);
}


vec3 calc_pbr_illumination(
    const float roughness,
    const float metallic,
    const vec3 albedo,
    const vec3 normal,
    const vec3 F0,
    const vec3 view_direc,
    const vec3 frag_to_light_direc,
    const vec3 light_color
) {
    const vec3 L = frag_to_light_direc;
    const vec3 H = normalize(view_direc + L);
    const vec3 radiance = light_color;
    const float NdotL = max(dot(normal, L), 0.0);

    const float NDF = distribution_ggx(normal, H, roughness);
    const float G = geometry_smith(normal, view_direc, L, roughness);
    const vec3 F = fresnel_schlick(clamp(dot(H, view_direc), 0.0, 1.0), F0);

    const vec3 nominator = NDF * G * F;
    const float denominator = 4 * max(dot(normal, view_direc), 0.0) * NdotL;
    const vec3 specular = nominator / max(denominator, 0.00001);

    const vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}


float phase_mie(const float cos_theta, const float anisotropy) {
    const float aa = anisotropy * anisotropy;
    const float numer = 3.0 * (1.0 - aa) * (1.0 + cos_theta * cos_theta);
    const float denom = 8.0*PI * (2.0 + aa) * (1.0 + aa - 2.0*anisotropy*cos_theta);
    return numer / denom;
}


float get_dither_value() {
    const float dither_pattern[16] = float[](
        0.0   , 0.5   , 0.125 , 0.625 ,
        0.75  , 0.22  , 0.875 , 0.375 ,
        0.1875, 0.6875, 0.0625, 0.5625,
        0.9375, 0.4375, 0.8125, 0.3125
    );

    const int i = int(gl_FragCoord.x) % 4;
    const int j = int(gl_FragCoord.y) % 4;
    return dither_pattern[4 * i + j];
}

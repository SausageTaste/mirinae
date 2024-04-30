#define PI 3.14159265359


float _distribution_GGX(const vec3 N, const vec3 H, const float roughness) {
    const float a = roughness*roughness;
    const float a2 = a*a;
    const float NdotH = max(dot(N, H), 0.0);
    const float NdotH2 = NdotH*NdotH;

    const float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.00001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}


float _geometry_SchlickGGX(const float NdotV, const float roughness) {
    const float r = (roughness + 1.0);
    const float k = (r*r) / 8.0;

    const float nom   = NdotV;
    const float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


float _geometry_Smith(const vec3 N, const vec3 V, const vec3 L, const float roughness) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    const float ggx2 = _geometry_SchlickGGX(NdotV, roughness);
    const float ggx1 = _geometry_SchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}


vec3 _fresnel_Schlick(const float cosTheta, const vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}


// https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
float _calc_attenuation(const float frag_distance, const float max_light_dist) {
    const float s = frag_distance / max_light_dist;

    if (s > 1.0)
        return 0.0;

    const float ss = s * s;
    return (1.0 - ss) * (1.0 - ss) / (1.0 + ss * 1.0);
}


vec3 calc_pbr_illumination(
    const float roughness,
    const float metallic,
    const vec3 albedo,
    const vec3 normal,
    const vec3 F0,
    const vec3 view_direc,
    const vec3 frag_to_light_direc,
    const float light_distance,
    const vec3 light_color
) {
    const vec3 L = frag_to_light_direc;
    const vec3 H = normalize(view_direc + L);
    const float dist = light_distance;
    const float attenuation = _calc_attenuation(dist, 15);
    const vec3 radiance = light_color * attenuation;

    const float NDF = _distribution_GGX(normal, H, roughness);
    const float G   = _geometry_Smith(normal, view_direc, L, roughness);
    const vec3 F    = _fresnel_Schlick(clamp(dot(H, view_direc), 0.0, 1.0), F0);

    const vec3 nominator    = NDF * G * F;
    const float denominator = 4 * max(dot(normal, view_direc), 0.0) * max(dot(normal, L), 0.0);
    const vec3 specular = nominator / max(denominator, 0.00001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0

    const vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    const float NdotL = max(dot(normal, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
}

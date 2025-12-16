module lighting;


static const float PI = 3.14159265358979323846;


public float distribution_ggx(const float3 N, const float3 H, const float roughness) {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float NdotH = max(dot(N, H), 0.0);
    const float NdotH2 = NdotH * NdotH;

    const float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.00001);
}


float geometry_schlick_ggx(const float NdotV, const float roughness) {
    const float r = (roughness + 1.0);
    const float k = (r * r) / 8.0;

    const float nom = NdotV;
    const float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


float geometry_smith(const float3 N, const float3 V, const float3 L, const float roughness) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    const float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    const float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}


float geometry_schlick_ggx_ibl(const float NdotV, const float roughness) {
    const float a = roughness;
    const float k = (a * a) / 2.0;

    const float nom = NdotV;
    const float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


public float geometry_smith_ibl(const float3 N, const float3 V, const float3 L, const float roughness) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    const float ggx2 = geometry_schlick_ggx_ibl(NdotV, roughness);
    const float ggx1 = geometry_schlick_ggx_ibl(NdotL, roughness);

    return ggx1 * ggx2;
}


float3 fresnel_schlick(const float cosTheta, const float3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}


float3 fresnel_schlick_rughness(float cosTheta, float3 F0, float roughness) {
    return (
        F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0)
    );
}


// https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
public float calc_attenuation(const float frag_distance, const float max_light_dist) {
    const float s = frag_distance / max_light_dist;

    if (s > 1.0)
        return 0.0;

    const float ss = s * s;
    return (1.0 - ss) * (1.0 - ss) / (1.0 + ss * 1.0);
}


public float calc_slight_attenuation(
    const float3 frag_pos,
    const float3 light_pos,
    const float3 light_direc,
    const float fade_start,
    const float fade_end
) {
    const float3 light_to_frag_n = normalize(frag_pos - light_pos);
    const float theta = dot(light_to_frag_n, light_direc);
    const float epsilon = fade_start - fade_end;
    const float atten_factor = 1;

    return clamp((theta - fade_end) / epsilon * atten_factor, 0.0, 1.0);
}


public float3 calc_pbr_illumination(
    const float roughness,
    const float metallic,
    const float3 albedo,
    const float3 normal,
    const float3 F0,
    const float3 view_direc,
    const float3 frag_to_light_direc,
    const float3 light_color
) {
    const float3 L = frag_to_light_direc;
    const float3 H = normalize(view_direc + L);
    const float3 radiance = light_color;
    const float NdotL = max(dot(normal, L), 0.0);

    const float NDF = distribution_ggx(normal, H, roughness);
    const float G = geometry_smith(normal, view_direc, L, roughness);
    const float3 F = fresnel_schlick(clamp(dot(H, view_direc), 0.0, 1.0), F0);

    const float3 nominator = NDF * G * F;
    const float denominator = 4 * max(dot(normal, view_direc), 0.0) * NdotL;
    const float3 specular = nominator / max(denominator, 0.00001);

    const float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}


public float3 ibl(
    const float3 normal_v,
    const float3 view_dir_v,
    const float3 albedo,
    const float3 f0,
    const float roughness,
    const float metallic,
    Sampler2D env_lut,
    SamplerCube env_diffuse,
    SamplerCube env_specular
) {
    const float3 N = normalize(normal_v);
    const float3 V = -normalize(view_dir_v);
    const float3 R = reflect(-V, N);
    const float NoV = max(dot(N, V), 0.0);
    const float3 F = fresnel_schlick_rughness(NoV, f0, roughness);

    const float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const float3 diffuse = env_diffuse.SampleLevel(N, 0).rgb * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    const float mip_lvl = roughness * MAX_REFLECTION_LOD;
    const float3 prefiltered_color = env_specular.SampleLevel(R, mip_lvl).rgb;
    const float2 env_brdf = env_lut.SampleLevel(float2(NoV, roughness), 0).rg;
    const float3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);

    return kD * diffuse + specular;
}


float phase_mie(const float cos_theta, const float anisotropy) {
    const float aa = anisotropy * anisotropy;
    const float numer = 3.0 * (1.0 - aa) * (1.0 + cos_theta * cos_theta);
    const float denom = 8.0 * PI * (2.0 + aa) * (1.0 + aa - 2.0 * anisotropy * cos_theta);
    return numer / denom;
}


public float3 calc_frag_pos(float depth, float2 uv, float4x4 proj_inv) {
    float4 clip_pos = float4(uv * 2 - 1, depth, 1);
    float4 frag_pos = mul(proj_inv, clip_pos);
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


float calc_depth(float3 frag_pos_v, float4x4 proj_mat) {
    const float4 clip_pos = mul(proj_mat, float4(frag_pos_v, 1));
    return clip_pos.z / clip_pos.w;
}


public float3x3 make_tbn_mat(float3 normal, float3 tangent, float3x3 m) {
    var T = normalize(mul(m, tangent));
    let N = normalize(mul(m, normal));
    T = normalize(T - dot(T, N) * N);
    let B = cross(N, T);
    return transpose(float3x3(T, B, N));
}

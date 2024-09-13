#version 450

#include "../utils/konst.glsl"
#include "../utils/lighting.glsl"
#include "common.glsl"


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec2 f_color;


vec2 integrate_brdf(float NdotV, float roughness) {
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    const vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        const vec2 Xi = hammersley(i, SAMPLE_COUNT);
        const vec3 H  = importance_sample_ggx(Xi, N, roughness);
        const vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(L.z, 0.0);
        const float NdotH = max(H.z, 0.0);
        const float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            const float G = geometry_smith_ibl(N, V, L, roughness);
            const float G_Vis = (G * VdotH) / (NdotH * NdotV);
            const float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}


void main() {
    f_color = integrate_brdf(v_uv_coord.x, v_uv_coord.y);
}

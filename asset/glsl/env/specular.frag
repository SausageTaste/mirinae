#version 450

#include "../utils/konst.glsl"
#include "../utils/lighting.glsl"
#include "common.glsl"


layout(location = 0) in vec3 v_local_pos;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform samplerCube u_envmap;

layout(push_constant) uniform U_EnvSpecularPushConst {
    mat4 proj_view;
    float roughness;
} u_push_const;


void main() {
    const vec3 N = normalize(v_local_pos);
    const vec3 R = N;
    const vec3 V = R;
    const uint SAMPLE_COUNT = 1024;

    float total_weight = 0.0;
    vec3 prefiltered_color = vec3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        const vec2 Xi = hammersley(i, SAMPLE_COUNT);
        const vec3 H = importance_sample_ggx(Xi, N, u_push_const.roughness);
        const vec3 L = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            const float D = distribution_ggx(N, H, u_push_const.roughness);
            const float NdotH = max(dot(N, H), 0.0);
            const float HdotV = max(dot(H, V), 0.0);
            const float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

            const float resolution = 256; // resolution of source cubemap face
            const float sa_texel = 4.0 * PI / (6.0 * resolution * resolution);
            const float sa_sample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            const float mipLevel = (u_push_const.roughness == 0.0)
                ? 0.0
                : 0.5 * log2(sa_sample / sa_texel);

            prefiltered_color += textureLod(u_envmap, L, mipLevel).rgb * NdotL;
            total_weight      += NdotL;
        }
    }
    prefiltered_color /= total_weight;

    f_color = vec4(prefiltered_color, 1.0);
}

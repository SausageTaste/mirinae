import "../module/konst";
import "../module/lighting";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float3 local_pos_;
};


[push_constant]
cbuffer U_EnvSpecularPushConst {
    float4x4 proj_view_;
    float roughness_;
}
u_pc;

layout(set = 0, binding = 0) SamplerCube u_envmap;


float radical_inverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;  // / 0x100000000
}


float2 hammersley(const uint i, const uint N) {
    return float2(float(i) / float(N), radical_inverse_VdC(i));
}


float3 importance_sample_ggx(const float2 Xi, const float3 N, const float roughness) {
    const float a = roughness * roughness;

    const float phi = 2.0 * PI * Xi.x;
    const float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    const float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sin_theta;
    H.y = sin(phi) * sin_theta;
    H.z = cos_theta;

    // from tangent-space vector to world-space sample vector
    const float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    const float3 tangent = normalize(cross(up, N));
    const float3 bitangent = cross(N, tangent);

    const float3 sample_vec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sample_vec);
}


[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    let local_pos = CUBE_VERTICES[vtxid];
    let clip_pos = mul(u_pc.proj_view_, float4(local_pos, 0));

    VSOutput output;
    output.local_pos_ = local_pos;
    output.pos_ = clip_pos.xyww;
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target {
    const float3 N = normalize(input.local_pos_);
    const float3 R = N;
    const float3 V = R;
    const uint SAMPLE_COUNT = 1024;

    float total_weight = 0.0;
    float3 prefiltered_color = float3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        const float2 Xi = hammersley(i, SAMPLE_COUNT);
        const float3 H = importance_sample_ggx(Xi, N, u_pc.roughness_);
        const float3 L = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            const float D = distribution_ggx(N, H, u_pc.roughness_);
            const float NdotH = max(dot(N, H), 0.0);
            const float HdotV = max(dot(H, V), 0.0);
            const float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

            const float resolution = 256;  // resolution of source cubemap face
            const float sa_texel = 4.0 * PI / (6.0 * resolution * resolution);
            const float sa_sample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            const float mipLevel = (u_pc.roughness_ == 0.0) ? 0.0
                                                            : 0.5 * log2(sa_sample / sa_texel);

            prefiltered_color += u_envmap.SampleLevel(L, mipLevel).rgb * NdotL;
            total_weight += NdotL;
        }
    }
    prefiltered_color /= total_weight;

    return float4(prefiltered_color, 1);
}

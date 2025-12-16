import "../module/envmap";
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

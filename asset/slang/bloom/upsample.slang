import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


[push_constant]
cbuffer U_BloomUpPushConst {
    float filter_radius_;
    float aspect_ratio_rcp_;
}
u_pc;

layout(set = 0, binding = 0) Sampler2D u_downsampled;


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    const float y = u_pc.filter_radius_;
    const float x = y * u_pc.aspect_ratio_rcp_;

    const float3 a = u_downsampled.Sample(input.texco_ + float2(-x, y)).rgb;
    const float3 b = u_downsampled.Sample(input.texco_ + float2(0, y)).rgb;
    const float3 c = u_downsampled.Sample(input.texco_ + float2(x, y)).rgb;

    const float3 d = u_downsampled.Sample(input.texco_ + float2(-x, 0)).rgb;
    const float3 e = u_downsampled.Sample(input.texco_).rgb;
    const float3 f = u_downsampled.Sample(input.texco_ + float2(x, 0)).rgb;

    const float3 g = u_downsampled.Sample(input.texco_ + float2(-x, -y)).rgb;
    const float3 h = u_downsampled.Sample(input.texco_ + float2(0, -y)).rgb;
    const float3 i = u_downsampled.Sample(input.texco_ + float2(x, -y)).rgb;

    var upsample = e * (4.0 / 16.0);
    upsample += (b + d + f + h) * (2.0 / 16.0);
    upsample += (a + c + g + i) * (1.0 / 16.0);
    return float4(upsample, 1);
}

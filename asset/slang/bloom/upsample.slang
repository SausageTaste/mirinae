import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


[push_constant]
cbuffer U_BloomUpPushConst {
    float filter_radius_;
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
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    let r = u_pc.filter_radius_;

    let level = 0;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = u_downsampled.SampleLevel(input.texco_ + float2(-r, r), level).rgb;
    float3 b = u_downsampled.SampleLevel(input.texco_ + float2(0, r), level).rgb;
    float3 c = u_downsampled.SampleLevel(input.texco_ + float2(r, r), level).rgb;

    float3 d = u_downsampled.SampleLevel(input.texco_ + float2(-r, 0), level).rgb;
    float3 e = u_downsampled.SampleLevel(input.texco_, level).rgb;
    float3 f = u_downsampled.SampleLevel(input.texco_ + float2(r, 0), level).rgb;

    float3 g = u_downsampled.SampleLevel(input.texco_ + float2(-r, -r), level).rgb;
    float3 h = u_downsampled.SampleLevel(input.texco_ + float2(0, -r), level).rgb;
    float3 i = u_downsampled.SampleLevel(input.texco_ + float2(r, -r), level).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    var upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;
    return float4(upsample, 1);
}

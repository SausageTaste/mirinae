import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


layout(set = 0, binding = 0) Sampler2D u_compo;


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = u_compo.SampleLevel(input.texco_, 0, int2(-2, 2)).rgb;
    float3 b = u_compo.SampleLevel(input.texco_, 0, int2(0, 2)).rgb;
    float3 c = u_compo.SampleLevel(input.texco_, 0, int2(2, 2)).rgb;

    float3 d = u_compo.SampleLevel(input.texco_, 0, int2(-2, 0)).rgb;
    float3 e = u_compo.SampleLevel(input.texco_, 0, int2(0, 0)).rgb;
    float3 f = u_compo.SampleLevel(input.texco_, 0, int2(2, 0)).rgb;

    float3 g = u_compo.SampleLevel(input.texco_, 0, int2(-2, -2)).rgb;
    float3 h = u_compo.SampleLevel(input.texco_, 0, int2(0, -2)).rgb;
    float3 i = u_compo.SampleLevel(input.texco_, 0, int2(2, -2)).rgb;

    float3 j = u_compo.SampleLevel(input.texco_, 0, int2(-1, 1)).rgb;
    float3 k = u_compo.SampleLevel(input.texco_, 0, int2(1, 1)).rgb;
    float3 l = u_compo.SampleLevel(input.texco_, 0, int2(-1, -1)).rgb;
    float3 m = u_compo.SampleLevel(input.texco_, 0, int2(1, -1)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    float3 downsample = e * 0.125;
    downsample += (a + c + g + i) * 0.03125;
    downsample += (b + d + f + h) * 0.0625;
    downsample += (j + k + l + m) * 0.125;

    return float4(downsample, 1);
}

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
    const float3 a = u_compo.SampleLevel(input.texco_, 0, int2(-2, 2)).rgb;
    const float3 b = u_compo.SampleLevel(input.texco_, 0, int2(0, 2)).rgb;
    const float3 c = u_compo.SampleLevel(input.texco_, 0, int2(2, 2)).rgb;

    const float3 d = u_compo.SampleLevel(input.texco_, 0, int2(-2, 0)).rgb;
    const float3 e = u_compo.SampleLevel(input.texco_, 0).rgb;
    const float3 f = u_compo.SampleLevel(input.texco_, 0, int2(2, 0)).rgb;

    const float3 g = u_compo.SampleLevel(input.texco_, 0, int2(-2, -2)).rgb;
    const float3 h = u_compo.SampleLevel(input.texco_, 0, int2(0, -2)).rgb;
    const float3 i = u_compo.SampleLevel(input.texco_, 0, int2(2, -2)).rgb;

    const float3 j = u_compo.SampleLevel(input.texco_, 0, int2(-1, 1)).rgb;
    const float3 k = u_compo.SampleLevel(input.texco_, 0, int2(1, 1)).rgb;
    const float3 l = u_compo.SampleLevel(input.texco_, 0, int2(-1, -1)).rgb;
    const float3 m = u_compo.SampleLevel(input.texco_, 0, int2(1, -1)).rgb;

    float3 downsample = e * (4.0 / 32.0);
    downsample += (a + c + g + i) * (1.0 / 32.0);
    downsample += (b + d + f + h) * (2.0 / 32.0);
    downsample += (j + k + l + m) * (4.0 / 32.0);
    return float4(downsample, 1);
}

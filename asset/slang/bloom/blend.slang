import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


[push_constant]
cbuffer U_BloomBlendPushConst {
    float strength_;
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
    let texel = u_downsampled.SampleLevel(input.texco_, 10);
    return float4(texel.xyz, 1);
}

import "../module/dither";


struct VSInput {
    float3 pos_;
    float2 texco_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
};


[push_constant]
cbuffer U_ShadowPushConst {
    float4x4 pvm_;
}
u_pc;

layout(set = 1, binding = 1) Sampler2D u_albedo_map;



[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_ = mul(u_pc.pvm_, float4(input.pos_, 1));
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
void frag_main(VSOutput input, float4 screen_pos: SV_Position) {
    let albedo_texel = u_albedo_map.Sample(input.texco_);
    let alpha = albedo_texel.a;
    let dithered = dither4x4(screen_pos.xy, pow(alpha, 1));
    if (dithered < 0.5)
        discard;
}

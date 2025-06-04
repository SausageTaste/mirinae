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


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_ = mul(u_pc.pvm_, float4(input.pos_, 1));
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
void frag_main() {}

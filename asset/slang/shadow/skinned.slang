import "../module/skinned";


struct VSInput {
    float3 pos_;
    float2 texco_;
    float4 j_weights_;
    int4 j_ids_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
};


struct U_GbufActorSkinned {
    float4x4 j_tforms_[MAX_JOINTS];
    float4x4 view_model_;
    float4x4 pvm_;
};

[push_constant]
cbuffer U_ShadowPushConst {
    float4x4 pvm_;
}
u_pc;

layout(set = 0, binding = 0) ConstantBuffer<U_GbufActorSkinned> u_gbuf_model;


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    let joint_mat = make_joint_transform(input.j_ids_, input.j_weights_, u_gbuf_model.j_tforms_);
    let after_joint = mul(joint_mat, float4(input.pos_, 1));

    VSOutput output;
    output.pos_ = mul(u_pc.pvm_, after_joint);
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
void frag_main() {}

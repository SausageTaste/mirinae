struct VSInput {
    float3 pos_;
    float3 normal_;
    float3 tangent_;
    float2 texco_;
    float4 j_weights_;
    int4 j_ids_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
};


static const int MAX_JOINTS = 256;

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


float4x4 make_joint_transform(int4 j_ids, float4 j_weights) {
    if (j_ids[0] < 0) {
        return float4x4(1);
    }

    float4x4 joint_transform = float4x4(0);
    for (int i = 0; i < 4; i++) {
        if (j_ids[i] < 0)
            break;

        joint_transform += j_weights[i] * u_gbuf_model.j_tforms_[j_ids[i]];
    }

    return joint_transform;
}


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    let joint_mat = make_joint_transform(input.j_ids_, input.j_weights_);
    let pvmj = mul(u_pc.pvm_, joint_mat);

    VSOutput output;
    output.pos_ = mul(pvmj, float4(input.pos_, 1));
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
void frag_main(VSOutput input) {}

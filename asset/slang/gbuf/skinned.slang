struct VSInput {
    float3 pos_;
    float3 normal_;
    float3 tangent_;
    float2 texco_;
    float4 j_weights_;
    int4 j_ids_;
}

struct VSOutput {
    float3x3 tbn_;
    float4 pos_ : SV_POSITION;
    float2 texco_;
};

struct FSOutput {
    float4 albedo_ : SV_Target0;
    float4 normal_ : SV_Target1;
    float4 material_ : SV_Target2;
};


static const int MAX_JOINTS = 256;

struct U_GbufModel {
    float roughness_;
    float metallic_;
};

struct U_GbufActorSkinned {
    float4x4 j_tforms_[MAX_JOINTS];
    float4x4 view_model_;
    float4x4 pvm_;
}

layout(set = 0, binding = 0) ConstantBuffer<U_GbufModel> u_model;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;
layout(set = 0, binding = 3) Sampler2D u_orm_map;
layout(set = 1, binding = 0) ConstantBuffer<U_GbufActorSkinned> u_gbuf_model;


float3x3 make_tbn_mat(float3 normal, float3 tangent, float3x3 m) {
    var T = normalize(mul(m, tangent));
    let N = normalize(mul(m, normal));
    T = normalize(T - dot(T, N) * N);
    let B = cross(N, T);
    return transpose(float3x3(T, B, N));
}


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
    let pvmj = mul(u_gbuf_model.pvm_, joint_mat);
    let vmj = mul(u_gbuf_model.view_model_, joint_mat);

    VSOutput output;
    output.tbn_ = make_tbn_mat(input.normal_, input.tangent_, float3x3(vmj));
    output.pos_ = mul(pvmj, float4(input.pos_, 1));
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
FSOutput frag_main(VSOutput input) {
    let albedo = u_albedo_map.Sample(input.texco_);
    let normal_texel = u_normal_map.Sample(input.texco_);
    let orm_texel = u_orm_map.Sample(input.texco_);

    var normal = normal_texel.xyz * 2 - 1;
    normal = mul(input.tbn_, normal);
    normal = normalize(normal) * 0.5 + 0.5;

    FSOutput output;
    output.albedo_ = float4(albedo.xyz, 1);
    output.normal_ = float4(normal, 0);
    output.material_[1] = saturate(u_model.roughness_ * orm_texel.y);
    output.material_[2] = saturate(u_model.metallic_ * orm_texel.z);
    return output;
}

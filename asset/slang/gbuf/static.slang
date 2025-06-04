struct VSInput {
    float3 pos_;
    float3 normal_;
    float3 tangent_;
    float2 texco_;
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


struct U_GbufModel {
    float roughness_;
    float metallic_;
};

struct U_GbufActor {
    float4x4 model_;
    float4x4 view_model_;
    float4x4 pvm_;
}

layout(set = 0, binding = 0) ConstantBuffer<U_GbufModel> u_model;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;
layout(set = 0, binding = 3) Sampler2D u_orm_map;
layout(set = 1, binding = 0) ConstantBuffer<U_GbufActor> u_gbuf_model;


float3x3 make_tbn_mat(float3 normal, float3 tangent, float3x3 m) {
    var T = normalize(mul(m, tangent));
    let N = normalize(mul(m, normal));
    T = normalize(T - dot(T, N) * N);
    let B = cross(N, T);
    return transpose(float3x3(T, B, N));
}


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.tbn_ = make_tbn_mat(input.normal_, input.tangent_, float3x3(u_gbuf_model.view_model_));
    output.pos_ = mul(u_gbuf_model.pvm_, float4(input.pos_, 1));
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

import "../module/lighting";


struct VSInput {
    float3 pos_;
    float3 normal_;
    float2 texco_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    // float3x3 tbn_;
    float3 light_;
    float2 texco_;
};


struct U_GbufModel {
    float roughness_;
    float metallic_;
};

struct U_GbufActor {
    float4x4 model_;
    float4x4 view_model_;
    float4x4 pvm_;
};

[push_constant]
cbuffer U_EnvmapPushConst {
    float4x4 proj_view_;
    float4 dlight_dir_;
    float4 dlight_color_;
}
u_pc;


layout(set = 0, binding = 0) ConstantBuffer<U_GbufModel> u_model;
layout(set = 1, binding = 0) ConstantBuffer<U_GbufActor> u_actor;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    let pvm = mul(u_pc.proj_view_, u_actor.model_);

    VSOutput output;
    output.pos_ = mul(pvm, float4(input.pos_, 1));
    // output.tbn_ = make_tbn_mat(input.normal_, input.tangent_, float3x3(u_actor.view_model_));
    output.light_ = 0.2;
    output.texco_ = input.texco_;

    const float3 normal = normalize(input.normal_);
    const float light_align = max(dot(normal, u_pc.dlight_dir_.xyz), 0);
    output.light_ += light_align * u_pc.dlight_color_.xyz * 0.8;

    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target {
    let albedo = u_albedo_map.Sample(input.texco_);
    // let normal_texel = u_normal_map.Sample(input.texco_);
    // let normal = mul(input.tbn_, (normal_texel.xyz * 2 - 1));
    return float4(albedo.rgb * input.light_, 1);
}

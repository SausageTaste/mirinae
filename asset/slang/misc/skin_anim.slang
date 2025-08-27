import "../module/skinned";


struct U_GbufActorSkinned {
    float4x4 j_tforms_[MAX_JOINTS];
    float4x4 view_model_;
    float4x4 pvm_;
}

layout(set = 0, binding = 0) ConstantBuffer<U_GbufActorSkinned> u_gbuf_model;
layout(set = 0, binding = 1) RWStructuredBuffer<float4> out_vertices;

[push_constant]
cbuffer U_SkinAnim {
    float time_;
}
u_pc;


[shader("compute")]
[numthreads(1, 1, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let idx = dispatch_id.x;
    out_vertices[idx] = float4(u_pc.time_, 0, 0, 1);
}

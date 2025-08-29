import "../module/lighting";
import "../module/skinned";


struct U_GbufActorSkinned {
    float4x4 j_tforms_[MAX_JOINTS];
    float4x4 view_model_;
    float4x4 pvm_;
}


layout(set = 0, binding = 0) RWStructuredBuffer<float> out_vertices;
layout(set = 0, binding = 1) StructuredBuffer<float> in_vertices;
layout(set = 0, binding = 2) ConstantBuffer<U_GbufActorSkinned> u_gbuf_model;

[push_constant]
cbuffer U_SkinAnim {
    float time_;
    int vertex_count_;
}
u_pc;


/*
struct VertexStatic {
    glm::vec3 pos_;
    glm::vec3 normal_;
    glm::vec3 tangent_;
    glm::vec2 texcoord_;
};


struct VertexSkinned {
    glm::ivec4 joint_indices_;
    glm::vec4 joint_weights_;
    glm::vec3 pos_;
    glm::vec3 normal_;
    glm::vec3 tangent_;
    glm::vec2 uv_;
};
*/


[shader("compute")]
[numthreads(128, 1, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let vertex_idx = dispatch_id.x;
    if (vertex_idx >= u_pc.vertex_count_)
        return;

    const int VTX_STATIC_SIZE = 3 + 3 + 3 + 2;
    const int VTX_SKINNED_SIZE = 4 + 4 + VTX_STATIC_SIZE;

    const int input_v_idx = vertex_idx * VTX_SKINNED_SIZE;
    let joint_indices = int4(
        reinterpret<int>(in_vertices[input_v_idx + 0]),
        reinterpret<int>(in_vertices[input_v_idx + 1]),
        reinterpret<int>(in_vertices[input_v_idx + 2]),
        reinterpret<int>(in_vertices[input_v_idx + 3])
    );
    let joint_weights = float4(
        in_vertices[input_v_idx + 4],
        in_vertices[input_v_idx + 5],
        in_vertices[input_v_idx + 6],
        in_vertices[input_v_idx + 7]
    );
    var pos = float3(
        in_vertices[input_v_idx + 8], in_vertices[input_v_idx + 9], in_vertices[input_v_idx + 10]
    );
    var normal = float3(
        in_vertices[input_v_idx + 11], in_vertices[input_v_idx + 12], in_vertices[input_v_idx + 13]
    );
    var tangent = float3(
        in_vertices[input_v_idx + 14], in_vertices[input_v_idx + 15], in_vertices[input_v_idx + 16]
    );
    let texco = float2(in_vertices[input_v_idx + 17], in_vertices[input_v_idx + 18]);

    let jmat = make_joint_transform(joint_indices, joint_weights, u_gbuf_model.j_tforms_);
    let jmat3 = float3x3(jmat);

    pos = mul(jmat, float4(pos, 1)).xyz;
    normal = normalize(mul(jmat3, normal));
    tangent = normalize(mul(jmat3, tangent));

    const int output_vtx_idx = vertex_idx * VTX_STATIC_SIZE;
    out_vertices[output_vtx_idx + 0] = pos.x;
    out_vertices[output_vtx_idx + 1] = pos.y;
    out_vertices[output_vtx_idx + 2] = pos.z;
    out_vertices[output_vtx_idx + 3] = normal.x;
    out_vertices[output_vtx_idx + 4] = normal.y;
    out_vertices[output_vtx_idx + 5] = normal.z;
    out_vertices[output_vtx_idx + 6] = tangent.x;
    out_vertices[output_vtx_idx + 7] = tangent.y;
    out_vertices[output_vtx_idx + 8] = tangent.z;
    out_vertices[output_vtx_idx + 9] = texco.x;
    out_vertices[output_vtx_idx + 10] = texco.y;
}

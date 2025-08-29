import "../module/lighting";


layout(set = 0, binding = 0) RWStructuredBuffer<float> out_vertices;
layout(set = 0, binding = 1) StructuredBuffer<float> in_vertices;
layout(set = 0, binding = 2) StructuredBuffer<float4x4> in_joint_palette;

[push_constant]
cbuffer U_SkinAnim {
    float time_;
    int vertex_count_;
}
u_pc;


const static int VTX_STATIC_SIZE = 3 + 3 + 3 + 2;
const static int VTX_SKINNED_SIZE = 4 + 4 + VTX_STATIC_SIZE;


public float4x4 make_joint_transform(int4 j_ids, float4 j_weights) {
    float4x4 joint_transform = float4x4(0);
    for (int i = 0; i < 4; i++) {
        let idx = clamp(j_ids[i], 0, 512);
        joint_transform += j_weights[i] * in_joint_palette[idx];
    }

    return joint_transform;
}


struct Fetcher {
    __init(int base_idx) {
        base_idx_ = base_idx;
    }

    float get_f(int offset) {
        return in_vertices[base_idx_ + offset];
    }

    float2 get_f2(int offset) {
        return float2(in_vertices[base_idx_ + offset], in_vertices[base_idx_ + offset + 1]);
    }

    float3 get_f3(int offset) {
        return float3(
            in_vertices[base_idx_ + offset],
            in_vertices[base_idx_ + offset + 1],
            in_vertices[base_idx_ + offset + 2]
        );
    }

    float4 get_f4(int offset) {
        return float4(
            in_vertices[base_idx_ + offset],
            in_vertices[base_idx_ + offset + 1],
            in_vertices[base_idx_ + offset + 2],
            in_vertices[base_idx_ + offset + 3]
        );
    }

    int get_i(int offset) {
        return reinterpret<int>(in_vertices[base_idx_ + offset]);
    }

    int4 get_i4(int offset) {
        return int4(
            reinterpret<int>(in_vertices[base_idx_ + offset]),
            reinterpret<int>(in_vertices[base_idx_ + offset + 1]),
            reinterpret<int>(in_vertices[base_idx_ + offset + 2]),
            reinterpret<int>(in_vertices[base_idx_ + offset + 3])
        );
    }

    private int base_idx_;
};


[shader("compute")]
[numthreads(128, 1, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let vertex_idx = dispatch_id.x;
    if (vertex_idx >= u_pc.vertex_count_)
        return;

    let fetcher = Fetcher(vertex_idx * VTX_SKINNED_SIZE);
    let j_indices = fetcher.get_i4(0);
    let j_weights = fetcher.get_f4(4);
    var pos = fetcher.get_f3(8);
    var normal = fetcher.get_f3(11);
    var tangent = fetcher.get_f3(14);
    let texco = fetcher.get_f2(17);

    let jmat = make_joint_transform(j_indices, j_weights);
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

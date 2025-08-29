module shadow;


static const float2 CASCADE_OFFSETS[4] = {
    { 0, 0 },
    { 0.5, 0 },
    { 0, 0.5 },
    { 0.5, 0.5 },
};


public uint select_cascade(float depth, float4 cascade_depths) {
    for (uint i = 0; i < 3; ++i) {
        if (cascade_depths[i] < depth) {
            return i;
        }
    }

    return 3;
}


public bool check_shadow_texco_range(const float3 texco) {
    if (texco.z < 0)
        return false;
    else if (texco.z > 1)
        return false;
    else if (texco.x < 0 || texco.x > 1 || texco.y < 0 || texco.y > 1)
        return false;
    else
        return true;
}


public float3 make_shadow_texco(float3 frag_pos_v, float4x4 light_mat) {
    let frag_pos_in_dlight = mul(light_mat, float4(frag_pos_v, 1));
    let proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    let sample_coord = proj_coords.xy * 0.5 + 0.5;
    return float3(sample_coord, proj_coords.z);
}


public float3 make_shadow_texco(float3 frag_pos_v, float4x4 light_mat, uint selected_cascade) {
    let frag_pos_in_dlight = mul(light_mat, float4(frag_pos_v, 1));
    let proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    let texco = (proj_coords.xy * 0.25 + 0.25) + CASCADE_OFFSETS[selected_cascade];
    return float3(texco, proj_coords.z);
}


public bool is_shadow_texco_in_range(const float3 texco) {
    if (texco.z < 0)
        return false;
    else if (texco.z > 1)
        return false;
    else if (texco.x < 0 || texco.x > 1 || texco.y < 0 || texco.y > 1)
        return false;
    else
        return true;
}

import "../module/lighting";


static const float2 FULLSCREEN_POS[3] = {
    { -1, -1 },
    { -1, 3 },
    { 3, -1 },
};

static const float2 FULLSCREEN_UV[3] = {
    { 0, 0 },
    { 0, 2 },
    { 2, 0 },
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


uniform Sampler2D u_depth_map;

[push_constant]
cbuffer U_CompoVplightPushConst {
    float4x4 proj_inv_;
    float4 light_pos_v_;
    float4 light_color_;
}
u_pc;


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    const float depth_texel = u_depth_map.Sample(input.texco_).r;

    const float3 frag_pos_v = calc_frag_pos(depth_texel, input.texco_, u_pc.proj_inv_);
    const float3 view_to_frag_v = normalize(frag_pos_v);

    const float3 light_pos_v = u_pc.light_pos_v_.xyz;
    const float3 view_to_light_v = light_pos_v;
    const float projected_light_distance = dot(view_to_light_v, view_to_frag_v);
    const float3 projected_light_pos_v = projected_light_distance * view_to_frag_v;

    const float h = distance(light_pos_v, projected_light_pos_v);
    const float a = dot(-projected_light_pos_v, view_to_frag_v);
    const float b = dot(frag_pos_v - projected_light_pos_v, view_to_frag_v);
    const float c = (atan(b / h) / h) - (atan(a / h) / h);
    const float3 color = u_pc.light_color_.xyz * c;

    return float4(color, 1);
}

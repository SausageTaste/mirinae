struct VSInput {
    float4 pos_;
    float4 color_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};

layout(set = 0, binding = 1) Sampler2D u_color_map;
layout(set = 0, binding = 2) Sampler2D u_mask_map;

[push_constant]
cbuffer U_OverlayPushConst {
    float4 color_;
    float2 pos_offset_;
    float2 pos_scale_;
    float2 uv_offset_;
    float2 uv_scale_;
}
u_pc;


static const float2 POSITIONS[6] = {
    { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 },
};


[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(POSITIONS[vtxid] * u_pc.pos_scale_ + u_pc.pos_offset_, 0, 1);
    output.texco_ = POSITIONS[vtxid] * u_pc.uv_scale_ + u_pc.uv_offset_;
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    let color_texel = u_color_map.Sample(input.texco_);
    let mask_texel = u_mask_map.Sample(input.texco_).r;

    var color = color_texel * u_pc.color_;
    color.a *= mask_texel;
    return color;
}

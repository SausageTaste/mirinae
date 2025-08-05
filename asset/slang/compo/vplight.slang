import "../module/konst";
import "../module/lighting";


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
    const float3 view_to_frag_nv = normalize(frag_pos_v);  // `nv` means normalized, in view space.

    const float3 light_pos_v = u_pc.light_pos_v_.xyz;  // Also, view to light vector
    const float projected_light_distance = dot(light_pos_v, view_to_frag_nv);
    const float3 projected_light_pos_v = projected_light_distance * view_to_frag_nv;

    // https://ijdykeman.github.io/graphics/simple_fog_shader
    const float h = distance(light_pos_v, projected_light_pos_v);
    const float a = dot(view_to_frag_nv, -projected_light_pos_v);
    const float b = dot(view_to_frag_nv, frag_pos_v - projected_light_pos_v);
    const float c = (atan(b / h) / h) - (atan(a / h) / h);
    const float3 color = u_pc.light_color_.xyz * c;

    return float4(color, 1);
}

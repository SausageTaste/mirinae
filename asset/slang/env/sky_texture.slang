import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float3 local_pos_;
};


[push_constant]
cbuffer U_EnvSkyPushConst {
    float4x4 proj_view_;
    float4 sun_dir_w_;
    float atmos_radius_bottom_;
}
u_pc;

layout(set = 0, binding = 0) Sampler2D u_equirectangular_map;


static const float2 invAtan = float2(0.1591, 0.3183);
float2 map_cube(float3 v) {
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    let pv3 = float3x3(u_pc.proj_view_);
    let local_pos = CUBE_VERTICES[vtxid];
    let clip_pos = mul(pv3, local_pos);

    VSOutput output;
    output.local_pos_ = local_pos;
    output.pos_ = float4(clip_pos.xy, 0, 1);
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target {
    let normal = normalize(input.local_pos_);
    let uv = map_cube(normal);
    let texel = u_equirectangular_map.SampleLevel(uv, 0);
    return float4(texel.rgb, 1);
}

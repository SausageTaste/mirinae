import "../module/complex";


layout(binding = 0, rgba32f) RWTexture2D<float4> ppong_images[6];
layout(binding = 1, rgba32f) RWTexture2D<float4> hkt_images[6];
layout(binding = 2) Sampler2D u_butterfly_cache;

[push_constant]
cbuffer U_OceanButterflyPushConst {
    int stage_;
    int pingpong_;
    int direction_;
}
u_pc;


float2 calc_h(float2 p, float2 q, float2 w) {
    return (Complex(w) * Complex(q) + Complex(p)).to_float2();
}


void horizontalButterflies(uint3 dispatch_id) {
    const int idx = int(dispatch_id.z);
    const int2 texel = int2(dispatch_id.xy);
    const float4 data = u_butterfly_cache[int2(u_pc.stage_, texel.x)];
    const int2 p_coord = int2(int(data.z), texel.y);
    const int2 q_coord = int2(int(data.w), texel.y);
    const float2 w = data.xy;

    if (u_pc.pingpong_ == 0) {
        const float4 p_texel = ppong_images[idx][p_coord];
        const float4 q_texel = ppong_images[idx][q_coord];
        hkt_images[idx][texel] = float4(
            calc_h(p_texel.xy, q_texel.xy, w), calc_h(p_texel.zw, q_texel.zw, w)
        );
    } else if (u_pc.pingpong_ == 1) {
        const float4 p_texel = hkt_images[idx][p_coord];
        const float4 q_texel = hkt_images[idx][q_coord];
        ppong_images[idx][texel] = float4(
            calc_h(p_texel.xy, q_texel.xy, w), calc_h(p_texel.zw, q_texel.zw, w)
        );
    }
}

void verticalButterflies(uint3 dispatch_id) {
    const int idx = int(dispatch_id.z);
    const int2 texel = int2(dispatch_id.xy);
    const float4 data = u_butterfly_cache[int2(u_pc.stage_, texel.y)];
    const int2 p_coord = int2(texel.x, int(data.z));
    const int2 q_coord = int2(texel.x, int(data.w));
    const float2 w = data.xy;

    if (u_pc.pingpong_ == 0) {
        const float4 p_texel = ppong_images[idx][p_coord];
        const float4 q_texel = ppong_images[idx][q_coord];
        hkt_images[idx][texel] = float4(
            calc_h(p_texel.xy, q_texel.xy, w), calc_h(p_texel.zw, q_texel.zw, w)
        );
    } else if (u_pc.pingpong_ == 1) {
        const float4 p_texel = hkt_images[idx][p_coord];
        const float4 q_texel = hkt_images[idx][q_coord];
        ppong_images[idx][texel] = float4(
            calc_h(p_texel.xy, q_texel.xy, w), calc_h(p_texel.zw, q_texel.zw, w)
        );
    }
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    if (u_pc.direction_ == 0) {
        horizontalButterflies(dispatch_id);
    } else if (u_pc.direction_ == 1) {
        verticalButterflies(dispatch_id);
    }
}

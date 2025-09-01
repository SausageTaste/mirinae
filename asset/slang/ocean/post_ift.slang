layout(binding = 0, rgba32f) RWTexture2D<float4> out_displacement[3];
layout(binding = 1, rgba32f) RWTexture2D<float4> out_derivative[3];
layout(binding = 2, rgba32f) RWTexture2D<float4> out_turbulence[3];
layout(binding = 3, rgba32f) RWTexture2D<float4> in_hkt_1[3];
layout(binding = 4, rgba32f) RWTexture2D<float4> in_hkt_2[3];

[push_constant]
cbuffer U_OceanFinalizePushConst {
    float2 hor_displace_scale;
    float dt;
    float turb_time_factor;
    int N;
}
u_pc;


float normalization_factor(int2 nm) {
    const float perms[2] = float[2](1.0, -1.0);
    const int index = int(fmod(nm.x + nm.y, 2));
    const float perm = perms[index];
    return perm;
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    const int2 nm = int2(dispatch_id.xy);
    const int cascade = int(dispatch_id.z);
    const float nor = normalization_factor(nm);

    const float4 hkt0_texel = nor * in_hkt_1[cascade][nm];
    const float4 hkt1_texel = nor * in_hkt_2[cascade][nm];

    const float dx = hkt0_texel.x;
    const float dz = hkt0_texel.y;
    const float dy = hkt0_texel.z;
    const float ddx_dx = hkt1_texel.z;
    const float ddy_dx = hkt1_texel.x;
    const float ddz_dx = hkt0_texel.w;
    const float ddy_dz = hkt1_texel.y;
    const float ddz_dz = hkt1_texel.w;
    const float2 lambda = u_pc.hor_displace_scale;

    out_displacement[cascade][nm] = float4(lambda.x * dx, dy, lambda.y * dz, 1);
    out_derivative[cascade][nm] = float4(ddy_dx, ddy_dz, ddx_dx * lambda.x, ddz_dz * lambda.y);

    float4 turb_texel = out_turbulence[cascade][nm];
    let jacobian = (1 + lambda.x * ddx_dx) * (1 + lambda.y * ddz_dz) -
                   lambda.x * lambda.y * ddz_dx * ddz_dx;
    let factor = turb_texel.r + u_pc.dt * u_pc.turb_time_factor / max(jacobian, 0.5);
    turb_texel.r = min(jacobian, factor);

    out_turbulence[cascade][nm] = turb_texel;
}

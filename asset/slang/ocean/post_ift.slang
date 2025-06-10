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

    const float4 texel_dxdy = nor * in_hkt_1[cascade][nm];
    const float4 texel_dz = nor * in_hkt_2[cascade][nm];

    const float2 DxDz = texel_dxdy.xy;
    const float2 DyDxz = texel_dxdy.zw;
    const float2 DyxDyz = texel_dz.xy;
    const float2 DxxDzz = texel_dz.zw;
    const float2 lambda = u_pc.hor_displace_scale;

    out_displacement[cascade][nm] = float4(lambda.x * DxDz.x, DyDxz.x, lambda.y * DxDz.y, 1);
    out_derivative[cascade][nm] = float4(DyxDyz, DxxDzz * lambda);

    float4 turb_texel = out_turbulence[cascade][nm];
    let jacobian = (1 + lambda.x * DxxDzz.x) * (1 + lambda.y * DxxDzz.y) -
                   lambda.x * lambda.y * DyDxz.y * DyDxz.y;
    let factor = turb_texel.r + u_pc.dt * u_pc.turb_time_factor / max(jacobian, 0.5);
    turb_texel.r = min(jacobian, factor);

    out_turbulence[cascade][nm] = turb_texel;
}

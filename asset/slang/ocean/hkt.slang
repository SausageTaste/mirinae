import "../module/complex";
import "../module/konst";
import "../module/ocean";


layout(binding = 0, rgba32f) RWTexture2D<float4> out_hkt_1[3];
layout(binding = 1, rgba32f) RWTexture2D<float4> out_hkt_2[3];
layout(binding = 2, rgba32f) RWTexture2D<float4> in_hk[3];

[push_constant]
cbuffer U_OceanTildeHktPushConst {
    float time;
    float repeat_time;
    float depth;
    int L[3];
    int N;
}
u_pc;


float dispersion(float mag) {
    const float w_0 = TAU / u_pc.repeat_time;
    const float w_k = calc_angular_frequency(mag, u_pc.depth);
    const float a = w_k / w_0;
    return floor(a) * w_0;
}


float4 pack_compl(Complex c1, Complex c2, Complex c3, Complex c4) {
    return float4(c1.add_i_times(c2).to_float2(), c3.add_i_times(c4).to_float2());
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    const int2 global_idx = int2(dispatch_id.xy);
    const int cascade = int(dispatch_id.z);

    const float2 x = make_spatial_pos(dispatch_id.xy, u_pc.N);
    const float2 k = make_wave_vector(x, u_pc.L[cascade]);
    const float mag = max(length(k), 0.00001);
    const float mag_rcp = 1.0 / mag;

    const float4 h0k_texel = in_hk[cascade][global_idx];
    const float w = dispersion(mag);
    const Complex exp_iwt = Complex::from_exp(w * u_pc.time);
    const Complex h = Complex(h0k_texel.xy) * exp_iwt + Complex(h0k_texel.zw) * exp_iwt.conj();
    const Complex ih = h.mul_i();
    const Complex minus_h = -h;

    const Complex Dx = ih * (k.x * mag_rcp);
    const Complex Dy = h;
    const Complex Dz = ih * (k.y * mag_rcp);

    const Complex dDx_dx = minus_h * (k.x * k.x * mag_rcp);  // Dx * i * k.x
    const Complex dDy_dx = ih * k.x;                         // Likewise
    const Complex dDz_dx = minus_h * (k.x * k.y * mag_rcp);

    const Complex dDy_dz = ih * k.y;
    const Complex dDz_dz = minus_h * (k.y * k.y * mag_rcp);

    out_hkt_1[cascade][global_idx] = pack_compl(Dx, Dz, Dy, dDz_dx);
    out_hkt_2[cascade][global_idx] = pack_compl(dDy_dx, dDy_dz, dDx_dx, dDz_dz);
}

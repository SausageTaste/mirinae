import "../module/complex";
import "../module/konst";


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
    const float w_k = sqrt(GRAVITY_EARTH * mag * tanh(mag * u_pc.depth));
    const float a = w_k / w_0;
    return floor(a) * w_0;
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    const int2 global_idx = int2(dispatch_id.xy);
    const int cascade = int(dispatch_id.z);

    const float2 x = float2(dispatch_id.xy) - float(u_pc.N) / 2.0;
    const float2 k = x * 2.0 * PI / float(u_pc.L[cascade]);
    float mag = length(k);
    float mag_rcp = 1.0 / mag;
    if (mag < 0.00001) {
        mag = 0.00001;
        mag_rcp = 1.0 / mag;
    }

    const float w = dispersion(mag);
    const Complex exp_iwt = Complex::from_exp(w * u_pc.time);
    const float4 h0k_texel = in_hk[cascade][global_idx];
    const Complex h = Complex(h0k_texel.xy) * exp_iwt + Complex(h0k_texel.zw) * exp_iwt.conj();
    const Complex ih = Complex(-h.imag, h.real);  // ih = i * h
    const Complex minus_h = h * -1.0;             // -h

    const Complex Dx = ih * (k.x * mag_rcp);
    const Complex Dy = h;
    const Complex Dz = ih * (k.y * mag_rcp);

    const Complex dDx_dx = minus_h * (k.x * k.x * mag_rcp);  // Dx * i * k.x
    const Complex dDy_dx = ih * k.x;                         // Likewise
    const Complex dDz_dx = minus_h * (k.x * k.y * mag_rcp);

    const Complex dDy_dz = ih * k.y;
    const Complex dDz_dz = minus_h * (k.y * k.y * mag_rcp);

    out_hkt_1[cascade][global_idx] = float4(
        Dx.add_i_times(Dz).to_float2(), Dy.add_i_times(dDz_dx).to_float2()
    );
    out_hkt_2[cascade][global_idx] = float4(
        dDy_dx.add_i_times(dDy_dz).to_float2(), dDx_dx.add_i_times(dDz_dz).to_float2()
    );
}

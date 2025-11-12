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


float make_tileable_omega(float k_len) {
    const float w_0 = TAU / u_pc.repeat_time;
    const float w_k = calc_angular_frequency(k_len, u_pc.depth);
    const float a = w_k / w_0;
    return floor(a) * w_0;
}


float4 pack_compl(Complex c1, Complex c2, Complex c3, Complex c4) {
    return float4(c1.add_i_times(c2).to_float2(), c3.add_i_times(c4).to_float2());
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let global_idx = dispatch_id.xy;
    let cascade = dispatch_id.z;

    const float2 k = make_wave_vector(dispatch_id.xy, u_pc.N, u_pc.L[cascade]);
    const float k_len_sqr = dot(k, k);
    if (0 == k_len_sqr) {
        out_hkt_1[cascade][global_idx] = 0;
        out_hkt_2[cascade][global_idx] = 0;
        return;
    }

    const float4 h0k_texel = in_hk[cascade][global_idx];
    const float k_len = sqrt(k_len_sqr);
    const float w = make_tileable_omega(k_len);
    const Complex exp_iwt = Complex::from_exp(w * u_pc.time);
    const Complex h = Complex(h0k_texel.xy) * exp_iwt + Complex(h0k_texel.zw) * exp_iwt.conj();

    const Complex Dx = h.mul_i() * (k.x / k_len);
    const Complex Dy = h;
    const Complex Dz = h.mul_i() * (k.y / k_len);

    const Complex dDx_dx = Dx.mul_i() * k.x;
    const Complex dDy_dx = Dy.mul_i() * k.x;
    const Complex dDz_dx = Dz.mul_i() * k.x;

    const Complex dDy_dz = Dy.mul_i() * k.y;
    const Complex dDz_dz = Dz.mul_i() * k.y;

    out_hkt_1[cascade][global_idx] = pack_compl(Dx, Dz, Dy, dDz_dx);
    out_hkt_2[cascade][global_idx] = pack_compl(dDy_dx, dDy_dz, dDx_dx, dDz_dz);
}

import "../module/complex";


layout(binding = 0, rgba32f) RWTexture2D<float4> out_hkt_1[3];
layout(binding = 1, rgba32f) RWTexture2D<float4> out_hkt_2[3];
layout(binding = 2, rgba32f) Texture2D<float4> in_hk[3];

[push_constant]
cbuffer U_OceanTildeHktPushConst {
    float time;
    float repeat_time;
    float depth;
    int L[3];
    int N;
}
u_pc;

static const float PI = 3.1415926535897932384626433832795028841971693993751058209749445923078;
static const float TAU = 6.283185307179586476925287;
static const float GRAVITY_EARTH = 9.81;


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
    let exp_iwt = Complex::from_exp(w * u_pc.time);
    let h0k_texel = in_hk[cascade][global_idx];
    let h = Complex(h0k_texel.xy) * exp_iwt + Complex(h0k_texel.zw) * exp_iwt.conj();
    let ih = Complex(-h.imag, h.real);  // ih = i * h
    let minus_h = h * -1.0;             // -h

    let Dx = ih * (k.x * mag_rcp);
    let Dy = h;
    let Dz = ih * (k.y * mag_rcp);

    let dDx_dx = minus_h * (k.x * k.x * mag_rcp);  // Dx * i * k.x
    let dDy_dx = ih * k.x;                       // Likewise
    let dDz_dx = minus_h * (k.x * k.y * mag_rcp);

    let dDy_dz = ih * k.y;
    let dDz_dz = minus_h * (k.y * k.y * mag_rcp);

    out_hkt_1[cascade][global_idx] = float4(
        float2(Dx.real - Dz.imag, Dx.imag + Dz.real),         // Dx + i * Dz
        float2(Dy.real - dDz_dx.imag, Dy.imag + dDz_dx.real)  // Dy + i * dDz_dx
    );

    out_hkt_2[cascade][global_idx] = float4(
        float2(dDy_dx.real - dDy_dz.imag, dDy_dx.imag + dDy_dz.real),  // dDy_dx + i * dDy_dz
        float2(dDx_dx.real - dDz_dz.imag, dDx_dx.imag + dDz_dz.real)   // dDx_dx + i * dDz_dz
    );
}

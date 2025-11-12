import "../module/complex";
import "../module/konst";
import "../module/ocean_jonswap";
import "../module/ocean";


layout(set = 0, binding = 0, rgba32f) RWTexture2D<float4> out_images[3];
layout(set = 0, binding = 1) Sampler2D u_noise;

[push_constant]
cbuffer U_OceanTildeHPushConst {
    float2 wind_dir;
    float wind_speed;
    float amplitude;
    float fetch;
    float depth;
    float swell;
    float spread_blend;
    float cutoff_high;
    float cutoff_low;
    float L;
    int N;
    int cascade;
}
u_pc;


float2 box_muller(float u, float v) {
    const float a = TAU * u;
    const float r = sqrt(-2.0 * log(max(v, 1e-6)));
    return float2(r * cos(a), r * sin(a));
}


float2 box_muller(float2 uv) {
    return box_muller(uv.x, uv.y);
}


bool should_be_cut_off(const float k_len) {
    return (k_len < u_pc.cutoff_low || k_len > u_pc.cutoff_high);
}


float4 make_texel(Complex a, Complex b) {
    return float4(a.to_float2(), b.to_float2());
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let k = make_wave_vector(dispatch_id.xy, u_pc.N, u_pc.L);
    let k_len_sqr = dot(k, k);
    if (0 == k_len_sqr) {
        out_images[u_pc.cascade][dispatch_id.xy] = 0;
        return;
    }

    let k_len = sqrt(k_len_sqr);
    if (should_be_cut_off(k_len)) {
        out_images[u_pc.cascade][dispatch_id.xy] = float4(0);
        return;
    }

    let noise_texel = u_noise.Load(int3(dispatch_id.xy, 0));
    let gause0 = Complex(box_muller(noise_texel.xy));
    let gause1 = Complex(box_muller(noise_texel.zw));
    let spectrum = JonswapCalculator(
        k_len,
        u_pc.amplitude,
        u_pc.depth,
        u_pc.fetch,
        u_pc.L,
        u_pc.spread_blend,
        u_pc.swell,
        u_pc.wind_speed,
        atan2(u_pc.wind_dir.y, u_pc.wind_dir.x)
    );

    out_images[u_pc.cascade][dispatch_id.xy] = make_texel(
        gause0 * (spectrum.calc(k) / sqrt(2)), gause1.conj() * (spectrum.calc(-k) / sqrt(2))
    );
}

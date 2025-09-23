import "../module/konst";


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

static const float GAMMA = 3.3;
static const float SHORT_WAVES_FADE = 0.01;


float4 gauseRnd(uint3 dispatch_id) {
    let noise_texel = u_noise.Load(int3(dispatch_id.xy, 0));
    const float u0 = frac(noise_texel.x), v0 = max(noise_texel.y, 1e-6);
    const float u1 = frac(noise_texel.z), v1 = max(noise_texel.w, 1e-6);
    const float a0 = TAU * u0, r0 = sqrt(-2.0 * log(v0));
    const float a1 = TAU * u1, r1 = sqrt(-2.0 * log(v1));
    return float4(r0 * cos(a0), r0 * sin(a0), r1 * cos(a1), r1 * sin(a1));
}


float sqr(float x) {
    return x * x;
}


float quad(float x) {
    let xx = x * x;
    return xx * xx;
}


float hexic(float x) {
    let xx = x * x;
    let xxxx = xx * xx;
    return xxxx * xx;
}


float octic(float x) {
    let xx = x * x;
    let xxxx = xx * xx;
    return xxxx * xxxx;
}


float2 phillips_spectrum(float2 wave_vector) {
    const float damping = 0.001;

    const float k = max(length(wave_vector), 0.00001);
    const float kk = k * k;
    const float gg = GRAVITY_EARTH * GRAVITY_EARTH;
    const float uuuu = quad(u_pc.wind_speed);

    const float k_dot_u = dot(normalize(wave_vector), normalize(u_pc.wind_dir));
    const float k_dot_u_conj = dot(normalize(-wave_vector), normalize(u_pc.wind_dir));

    const float spectrum = (u_pc.amplitude / (kk * kk)) * exp(-gg / (kk * uuuu));
    const float suppression = exp(-kk * uuuu * damping * damping / gg);

    const float h0k = sqrt(spectrum * suppression * hexic(k_dot_u));

    const float h0minusk = sqrt(spectrum * suppression * hexic(k_dot_u_conj));

    return float2(h0k, h0minusk) / sqrt(2.0);
}


float Frequency(float k) {
    return sqrt(GRAVITY_EARTH * k * tanh(min(k * u_pc.depth, 20)));
}


float FrequencyDerivative(float k) {
    const float kh = min(k * u_pc.depth, 20);
    const float th = tanh(kh);
    const float ch = cosh(kh);
    return GRAVITY_EARTH * (u_pc.depth * k / (ch * ch) + th) / (2 * Frequency(k));
}


float NormalisationFactor(float s) {
    let s2 = s * s;
    let s3 = s2 * s;
    let s4 = s3 * s;
    if (s < 5)
        return -0.000564 * s4 + 0.00776 * s3 - 0.044 * s2 + 0.192 * s + 0.163;
    else
        return -4.80e-08 * s4 + 1.07e-05 * s3 - 9.53e-04 * s2 + 5.90e-02 * s + 3.93e-01;
}


float DonelanBannerBeta(float x) {
    if (x < 0.95)
        return 2.61 * pow(abs(x), 1.3);
    if (x < 1.6)
        return 2.28 * pow(abs(x), -1.3);
    let p = -0.4 + 0.8393 * exp(-0.567 * log(x * x));
    return pow(10, p);
}


float DonelanBanner(float theta, float omega, float peakOmega) {
    let beta = DonelanBannerBeta(omega / peakOmega);
    let sech = 1 / cosh(beta * theta);
    return beta / 2 / tanh(beta * 3.1416) * sech * sech;
}


float Cosine2s(float theta, float s) {
    return NormalisationFactor(s) * pow(abs(cos(0.5 * theta)), 2 * s);
}


float SpreadPower(float omega, float peak_omega) {
    if (omega > peak_omega) {
        return 9.77 * pow(abs(omega / peak_omega), -2.5);
    } else {
        return 6.97 * pow(abs(omega / peak_omega), 5);
    }
}


float DirectionSpectrum(float theta, float omega, float peak_omega) {
    let wind_angle = atan2(u_pc.wind_dir.y, u_pc.wind_dir.x);
    let s = SpreadPower(omega, peak_omega) +
            16.0 * tanh(min(omega / peak_omega, 20.0)) * u_pc.swell * u_pc.swell;
    return lerp(
        2 / PI * cos(theta) * cos(theta), Cosine2s(theta - wind_angle, s), u_pc.spread_blend
    );
}


float TMACorrection(float omega) {
    let omegaH = omega * sqrt(u_pc.depth / GRAVITY_EARTH);
    if (omegaH <= 1)
        return 0.5 * omegaH * omegaH;
    if (omegaH < 2)
        return 1.0 - 0.5 * (2.0 - omegaH) * (2.0 - omegaH);
    return 1;
}


float JONSWAP(float omega, float peak_omega) {
    let g = GRAVITY_EARTH;
    let u = u_pc.wind_speed;
    let alpha = 0.076 * pow(g * u_pc.fetch / sqr(u), -0.22);

    float sigma;
    if (omega <= peak_omega)
        sigma = 0.07;
    else
        sigma = 0.09;
    let r = exp(
        -(omega - peak_omega) * (omega - peak_omega) / 2.0 / sigma / sigma / peak_omega / peak_omega
    );

    let oneOverOmega = 1.0 / omega;
    let peakOmegaOverOmega = peak_omega / omega;
    return u_pc.amplitude * TMACorrection(omega) * alpha * g * g * oneOverOmega * oneOverOmega *
           oneOverOmega * oneOverOmega * oneOverOmega *
           exp(-1.25 * peakOmegaOverOmega * peakOmegaOverOmega * peakOmegaOverOmega *
               peakOmegaOverOmega) *
           pow(abs(GAMMA), r);
}


float ShortWavesFade(float k) {
    return exp(-SHORT_WAVES_FADE * SHORT_WAVES_FADE * k * k);
}


float jonswap_spectrum(float2 wave_vector) {
    const float deltaK = TAU / float(u_pc.L);
    const float k_len = max(length(wave_vector), 0.000001);

    if (k_len < u_pc.cutoff_low || k_len > u_pc.cutoff_high)
        return 0;

    let gg = GRAVITY_EARTH * GRAVITY_EARTH;
    const float k_angle = atan2(wave_vector.y, wave_vector.x);
    const float peak_omega = 22 * pow(gg / (u_pc.wind_speed * u_pc.fetch), 0.33);
    const float omega = Frequency(k_len);
    const float4 wave_data = float4(wave_vector.x, 1 / k_len, wave_vector.y, omega);
    const float domega_dk = FrequencyDerivative(k_len);

    const float spectrum = JONSWAP(omega, peak_omega) *
                           DirectionSpectrum(k_angle, omega, peak_omega) * ShortWavesFade(k_len);

    return sqrt(2 * spectrum * abs(domega_dk) / k_len * deltaK * deltaK);
}


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let x = float2(dispatch_id.xy) - float(u_pc.N) * 0.5;
    let k = x * (TAU / float(u_pc.L));

    let gause_random = gauseRnd(dispatch_id);

    out_images[u_pc.cascade][dispatch_id.xy] = float4(
        gause_random.xy * jonswap_spectrum(k), gause_random.zw * jonswap_spectrum(-k)
    );
}

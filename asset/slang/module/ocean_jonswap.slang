module ocean_jonswap;

import "../module/konst";
import "../module/ocean";


static const float GAMMA = 3.3;
static const float SHORT_WAVES_FADE = 0.01;


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


float FrequencyDerivative(float k, float depth) {
    const float kh = min(k * depth, 20);
    const float th = tanh(kh);
    const float ch = cosh(kh);
    return GRAVITY_EARTH * (depth * k / (ch * ch) + th) / (2 * calc_angular_frequency(k, depth));
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


float DirectionSpectrum(float theta, float spread_power, float spread_blend, float2 wind_dir) {
    let wind_angle = atan2(wind_dir.y, wind_dir.x);
    return lerp(
        2 / PI * cos(theta) * cos(theta), Cosine2s(theta - wind_angle, spread_power), spread_blend
    );
}


float DirectionSpectrum(
    float theta, float omega, float peak_omega, float spread_blend, float swell, float2 wind_dir
) {
    let s = SpreadPower(omega, peak_omega) +
            16.0 * tanh(min(omega / peak_omega, 20.0)) * swell * swell;
    return DirectionSpectrum(theta, s, spread_blend, wind_dir);
}


float TMACorrection(float omega, float depth) {
    let omegaH = omega * sqrt(depth / GRAVITY_EARTH);
    if (omegaH <= 1)
        return 0.5 * omegaH * omegaH;
    if (omegaH < 2)
        return 1.0 - 0.5 * (2.0 - omegaH) * (2.0 - omegaH);
    return 1;
}


float JONSWAP(
    float omega, float peak_omega, float depth, float wind_speed, float fetch, float amplitude
) {
    let g = GRAVITY_EARTH;
    let u = wind_speed;
    let alpha = 0.076 * pow(g * fetch / sqr(u), -0.22);

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
    return amplitude * TMACorrection(omega, depth) * alpha * g * g * oneOverOmega * oneOverOmega *
           oneOverOmega * oneOverOmega * oneOverOmega *
           exp(-1.25 * peakOmegaOverOmega * peakOmegaOverOmega * peakOmegaOverOmega *
               peakOmegaOverOmega) *
           pow(abs(GAMMA), r);
}


float ShortWavesFade(float k) {
    return exp(-SHORT_WAVES_FADE * SHORT_WAVES_FADE * k * k);
}


public struct JonswapCalculator {
    public __init(
        float k_len,
        float amplitude,
        float depth,
        float fetch,
        float L,
        float spread_blend,
        float swell,
        float wind_speed,
        float2 wind_dir
    ) {
        const float delta_k = TAU / L;
        const float domega_dk = FrequencyDerivative(k_len, depth);
        const float omega = calc_angular_frequency(k_len, depth);

        const float gg = GRAVITY_EARTH * GRAVITY_EARTH;
        const float peak_omega = 22 * pow(gg / (wind_speed * fetch), 0.33);

        const float jonswap = JONSWAP(omega, peak_omega, depth, wind_speed, fetch, amplitude);
        const float short_wave_fade = ShortWavesFade(k_len);

        spectrum_ = 2.0 * abs(domega_dk) / k_len * delta_k * delta_k * jonswap * short_wave_fade;

        spread_power_ = SpreadPower(omega, peak_omega) +
                        16.0 * tanh(min(omega / peak_omega, 20.0)) * swell * swell;

        spread_blend_ = spread_blend;
        wind_dir_ = wind_dir;
    }

    public float calc(float2 wave_vector) {
        const float k_angle = atan2(wave_vector.y, wave_vector.x);
        return sqrt(
            DirectionSpectrum(k_angle, spread_power_, spread_blend_, wind_dir_) * spectrum_
        );
    }

    // Input variables
    private float2 wind_dir_;
    private float spread_blend_;
    // Precomputed variables
    private float spectrum_;
    private float spread_power_;
}

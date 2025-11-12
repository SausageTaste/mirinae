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


float pentic(float x) {
    let xx = x * x;
    let xxxx = xx * xx;
    return xxxx * x;
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


/*
float DirectionSpectrum(
    float theta, float omega, float peak_omega, float spread_blend, float swell, float wind_angle
) {
    let s = SpreadPower(omega, peak_omega) +
            16.0 * tanh(min(omega / peak_omega, 20.0)) * swell * swell;
    return DirectionSpectrum(theta, s, spread_blend, wind_angle);
}
*/


float TMACorrection(float omega, float depth) {
    let omegaH = omega * sqrt(depth / GRAVITY_EARTH);
    if (omegaH <= 1)
        return 0.5 * omegaH * omegaH;
    if (omegaH < 2)
        return 1.0 - 0.5 * (2.0 - omegaH) * (2.0 - omegaH);
    return 1;
}


// Second formula from
// https://www.codecogs.com/library/engineering/fluid_mechanics/waves/spectra/jonswap.php
float calc_alpha(float wind_speed, float fetch, float peak_omega) {
    /*/
    return 0.076 * pow(GRAVITY_EARTH * fetch / sqr(wind_speed), -0.22);
    /*/
    return 0.033 * pow(wind_speed * peak_omega / GRAVITY_EARTH, 2.0 / 3.0);
    //*/
}


float calc_peak_omega(float wind_speed, float fetch) {
    /*/
    return 22 * pow(GRAVITY_EARTH * GRAVITY_EARTH / (wind_speed * fetch), 0.33);
    /*/
    return 2.84 * pow(GRAVITY_EARTH, 0.7) / (pow(fetch, 0.3) * pow(wind_speed, 0.4));
    //*/
}


float JONSWAP(
    float omega, float peak_omega, float depth, float wind_speed, float fetch, float amplitude
) {
    let sigma = (omega <= peak_omega) ? 0.07 : 0.09;
    let alpha = calc_alpha(wind_speed, fetch, peak_omega);

    /*
    let r = exp(
        -(omega - peak_omega) * (omega - peak_omega) / 2.0 / sigma / sigma / peak_omega / peak_omega
    );
    */
    let r = exp(-0.5 * sqr((omega / peak_omega) - 1) / (sigma * sigma));

    let omega_rcp = 1 / omega;
    return amplitude * TMACorrection(omega, depth) * alpha * sqr(GRAVITY_EARTH) *
           pentic(omega_rcp) * exp(-1.25 * quad(peak_omega * omega_rcp)) * pow(GAMMA, r);
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
        float wind_angle
    ) {
        const float delta_k = TAU / L;
        const float domega_dk = FrequencyDerivative(k_len, depth);
        const float omega = calc_angular_frequency(k_len, depth);
        const float peak_omega = calc_peak_omega(wind_speed, fetch);

        const float jonswap = JONSWAP(omega, peak_omega, depth, wind_speed, fetch, amplitude);
        const float short_wave_fade = ShortWavesFade(k_len);

        spectrum_ = 2.0 * abs(domega_dk) / k_len * delta_k * delta_k * jonswap * short_wave_fade;

        spread_power_ = SpreadPower(omega, peak_omega) +
                        16.0 * tanh(min(omega / peak_omega, 20.0)) * swell * swell;

        normalisation_factor_ = NormalisationFactor(spread_power_);

        spread_blend_ = spread_blend;
        wind_angle_ = wind_angle;
    }

    public float calc(float2 wave_vector) {
        const float k_angle = atan2(wave_vector.y, wave_vector.x);
        const float cos_2s = normalisation_factor_ *
                             pow(abs(cos(0.5 * (k_angle - wind_angle_))), 2 * spread_power_);
        const float direction_spectrum = lerp(2 / PI * sqr(cos(k_angle)), cos_2s, spread_blend_);
        return sqrt(direction_spectrum * spectrum_);
    }

    // Input variables
    private float wind_angle_;
    private float spread_blend_;
    // Precomputed variables
    private float spectrum_;
    private float spread_power_;
    private float normalisation_factor_;
}

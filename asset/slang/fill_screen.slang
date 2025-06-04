
static const float2 FULLSCREEN_POS[3] = {
    { -1, -1 },
    { 3, -1 },
    { -1, 3 },
};

static const float2 FULLSCREEN_UV[3] = {
    { 0, 0 },
    { 2, 0 },
    { 0, 2 },
};


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


uniform Sampler2D u_compo_image;

[push_constant]
cbuffer U_FillScreenPushConst {
    float exposure_;
    float gamma_;
}
u_pc;


float luminance(float3 v) {
    return dot(v, float3(0.2126, 0.7152, 0.0722));
}


float3 reinhard_jodie(float3 v) {
    let l = luminance(v);
    let tv = v / (1.0 + v);
    return lerp(v / (1.0 + l), tv, tv);
}


float3 _uncharted2_tonemap_partial(float3 x) {
    let A = 0.15;
    let B = 0.50;
    let C = 0.10;
    let D = 0.20;
    let E = 0.02;
    let F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}


float3 uncharted2_filmic(float3 v) {
    let exposure_bias = 2.0;
    let curr = _uncharted2_tonemap_partial(v * exposure_bias);

    let W = float3(11.2);
    let white_scale = float3(1.0f) / _uncharted2_tonemap_partial(W);
    return curr * white_scale;
}


float3 _rtt_and_odt_fit(float3 v) {
    let a = v * (v + 0.0245786) - 0.000090537;
    let b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}


float3 aces_fitted(float3 v) {
    let aces_input_matrix = float3x3(
        float3(0.59719, 0.07600, 0.02840),
        float3(0.35458, 0.90834, 0.13383),
        float3(0.04823, 0.01566, 0.83777)
    );

    let aces_output_matrix = float3x3(
        float3(1.60475, -0.10208, -0.00327),
        float3(-0.53108, 1.10813, -0.07276),
        float3(-0.07367, -0.00605, 1.07602)
    );

    v = mul(aces_input_matrix, v);
    v = _rtt_and_odt_fit(v);
    return mul(aces_output_matrix, v);
}


float3 aces_approx(float3 v) {
    v *= 0.6;
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((v * (a * v + b)) / (v * (c * v + d) + e), 0.0, 1.0);
}


float3 khronos_pbr_neutral(float3 color) {
    let startCompression = 0.8 - 0.04;
    let desaturation = 0.15;

    let x = min(color.r, min(color.g, color.b));
    let offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    let peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
        return color;

    let d = 1. - startCompression;
    let newPeak = 1. - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    let g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
    return lerp(color, newPeak * float3(1), g);
}


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    var color = u_compo_image.Sample(input.texco_).xyz;
    color = float3(1) - exp(-color * u_pc.exposure_);
    color = aces_approx(color);
    color = pow(color, float3(1 / u_pc.gamma_));
    return float4(color, 1);
}

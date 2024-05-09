
float luminance(vec3 v) {
    return dot(v, vec3(0.2126, 0.7152, 0.0722));
}

vec3 reinhard_jodie(vec3 v) {
    float l = luminance(v);
    vec3 tv = v / (1.0 + v);
    return mix(v / (1.0 + l), tv, tv);
}


vec3 _uncharted2_tonemap_partial(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 uncharted2_filmic(vec3 v) {
    float exposure_bias = 2;
    vec3 curr = _uncharted2_tonemap_partial(v * exposure_bias);

    vec3 W = vec3(11.2);
    vec3 white_scale = vec3(1.0f) / _uncharted2_tonemap_partial(W);
    return curr * white_scale;
}


vec3 _rtt_and_odt_fit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 aces_fitted(vec3 v) {
    mat3 aces_input_matrix = mat3(
        vec3(0.59719, 0.07600, 0.02840),
        vec3(0.35458, 0.90834, 0.13383),
        vec3(0.04823, 0.01566, 0.83777)
    );

    mat3 aces_output_matrix = mat3(
        vec3( 1.60475, -0.10208, -0.00327),
        vec3(-0.53108,  1.10813, -0.07276),
        vec3(-0.07367, -0.00605,  1.07602)
    );

    v = aces_input_matrix * v;
    v = _rtt_and_odt_fit(v);
    return aces_output_matrix * v;
}


vec3 aces_approx(vec3 v) {
    v *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0, 1.0);
}

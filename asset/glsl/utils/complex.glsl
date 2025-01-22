struct Complex {
    float re;
    float im;
};

Complex complex_init() {
    return Complex(0, 0);
}

Complex complex_init(float real, float img) {
    return Complex(real, img);
}

Complex complex_init(vec2 real_img) {
    return Complex(real_img.x, real_img.y);
}

Complex complex_init_exp(float theta) {
    return Complex(cos(theta), sin(theta));
}

Complex complex_mul(Complex a, Complex b) {
    return Complex(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

Complex complex_add(Complex a, Complex b) {
    return Complex(a.re + b.re, a.im + b.im);
}

Complex complex_conj(Complex a) {
    return Complex(a.re, -a.im);
}

vec2 complex_to_vec2(Complex a) {
    return vec2(a.re, a.im);
}

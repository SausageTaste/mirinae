struct Complex {
    float re;
    float im;
};

Complex complex_mul(Complex a, Complex b) {
    return Complex(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

Complex complex_add(Complex a, Complex b) {
    return Complex(a.re + b.re, a.im + b.im);
}

Complex complex_conj(Complex a) {
    return Complex(a.re, -a.im);
}

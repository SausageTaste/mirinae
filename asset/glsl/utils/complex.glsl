vec2 complex_init() {
    return vec2(0, 0);
}

vec2 complex_init(float real, float img) {
    return vec2(real, img);
}

// This doesn't do anything but good for readability
vec2 complex_init(vec2 real_img) {
    return real_img;
}

vec2 complex_init_exp(float theta) {
    return vec2(cos(theta), sin(theta));
}

vec2 complex_mul(vec2 a, vec2 b) {
    return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

vec2 complex_conj(vec2 a) {
    return vec2(a.x, -a.y);
}

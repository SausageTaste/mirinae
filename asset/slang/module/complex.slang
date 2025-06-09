module complex;


public struct Complex {
    public __init() {
        value_ = float2(0.0, 0.0);
    }

    public __init(float real, float imag) {
        value_ = float2(real, imag);
    }

    public __init(float2 real_imag) {
        value_ = real_imag;
    }

    public static Complex from_exp(float angle) {
        return Complex(cos(angle), sin(angle));
    }

    public property float real {
        get { return value_.x; }
        set { value_.x = newValue; }
    }

    public property float imag {
        get { return value_.y; }
        set { value_.y = newValue; }
    }

    public Complex conj() {
        return Complex(value_.x, -value_.y);
    }

    private float2 value_;
};


public Complex operator +(Complex a, Complex b) {
    return Complex(a.real + b.real, a.imag + b.imag);
}


public Complex operator*(Complex a, float b) {
    return Complex(a.real * b, a.imag * b);
}


public Complex operator*(Complex a, Complex b) {
    return Complex(a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real);
}

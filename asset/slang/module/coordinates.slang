module coordinates;


struct Aabb1D {
    [mutating]
    void set(float a) {
        min_ = a;
        max_ = a;
    }

    [mutating]
    void set(float a, float b) {
        min_ = min(a, b);
        max_ = max(a, b);
    }

    [mutating]
    void expand(float a) {
        min_ = min(min_, a);
        max_ = max(max_, a);
    }

    bool is_intersecting_op(Aabb1D rhs) {
        return max_ > rhs.min_ && min_ < rhs.max_;
    }

    bool is_intersecting_cl(Aabb1D rhs) {
        return max_ >= rhs.min_ && min_ <= rhs.max_;
    }

    private float min_;
    private float max_;
}


struct Aabb2D {
    [mutating]
    void set(float2 a) {
        x_.set(a.x);
        y_.set(a.y);
    }

    [mutating]
    void set(float2 a, float2 b) {
        x_.set(a.x, b.x);
        y_.set(a.y, b.y);
    }

    [mutating]
    void expand(float2 a) {
        x_.expand(a.x);
        y_.expand(a.y);
    }

    bool is_intersecting_op(Aabb2D rhs) {
        return x_.is_intersecting_op(rhs.x_) && y_.is_intersecting_op(rhs.y_);
    }

    bool is_intersecting_cl(Aabb2D rhs) {
        return x_.is_intersecting_cl(rhs.x_) && y_.is_intersecting_cl(rhs.y_);
    }

    private Aabb1D x_;
    private Aabb1D y_;
}


float4 persp_mul(float4x4 m, float4 v) {
    var p = mul(m, v);
    p /= p.w;
    return p;
}

public float4 persp_mul(float4x4 m, float3 v) {
    return persp_mul(m, float4(v, 1));
}

public bool is_is_screen(float2[] points) {
    let SCREEN_POINTS = float2[](float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, 1));

    Aabb2D screen_aabb;
    screen_aabb.set(float2(-1, -1), float2(1, 1));

    Aabb2D points_aabb;
    points_aabb.set(points[0]);
    for (int i = 1; i < points.getCount(); ++i) {
        points_aabb.expand(points[i]);
    }

    return screen_aabb.is_intersecting_cl(points_aabb);
}

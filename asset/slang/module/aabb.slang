module aabb;


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


public struct Aabb2D {
    public __init(float2 a) {
        this.set(a);
    }

    public __init(float2 a, float2 b) {
        this.set(a, b);
    }

    public __init(float2[] points) {
        this.set(points);
    }

    [mutating]
    public void set(float2 a) {
        x_.set(a.x);
        y_.set(a.y);
    }

    [mutating]
    public void set(float2 a, float2 b) {
        x_.set(a.x, b.x);
        y_.set(a.y, b.y);
    }

    [mutating]
    public void set(float2[] points) {
        this.set(points[0]);
        for (int i = 1; i < points.getCount(); ++i) {
            this.expand(points[i]);
        }
    }

    [mutating]
    public void expand(float2 a) {
        x_.expand(a.x);
        y_.expand(a.y);
    }

    public bool is_intersecting_op(Aabb2D rhs) {
        return x_.is_intersecting_op(rhs.x_) && y_.is_intersecting_op(rhs.y_);
    }

    public bool is_intersecting_cl(Aabb2D rhs) {
        return x_.is_intersecting_cl(rhs.x_) && y_.is_intersecting_cl(rhs.y_);
    }

    private Aabb1D x_;
    private Aabb1D y_;
}


public struct Aabb3D {
    public __init(float3 a) {
        this.set(a);
    }

    public __init(float3 a, float3 b) {
        this.set(a, b);
    }

    public __init(float3[] points) {
        this.set(points);
    }

    [mutating]
    public void set(float3 a) {
        x_.set(a.x);
        y_.set(a.y);
        z_.set(a.z);
    }

    [mutating]
    public void set(float3 a, float3 b) {
        x_.set(a.x, b.x);
        y_.set(a.y, b.y);
        z_.set(a.z, b.z);
    }

    [mutating]
    public void set(float3[] points) {
        this.set(points[0]);
        for (int i = 1; i < points.getCount(); ++i) {
            this.expand(points[i]);
        }
    }

    [mutating]
    public void expand(float3 a) {
        x_.expand(a.x);
        y_.expand(a.y);
        z_.expand(a.z);
    }

    public bool is_intersecting_op(Aabb3D rhs) {
        return x_.is_intersecting_op(rhs.x_) && y_.is_intersecting_op(rhs.y_) &&
               z_.is_intersecting_op(rhs.z_);
    }

    public bool is_intersecting_cl(Aabb3D rhs) {
        return x_.is_intersecting_cl(rhs.x_) && y_.is_intersecting_cl(rhs.y_) &&
               z_.is_intersecting_cl(rhs.z_);
    }

    private Aabb1D x_;
    private Aabb1D y_;
    private Aabb1D z_;
}

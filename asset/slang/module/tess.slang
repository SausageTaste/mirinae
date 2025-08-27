module tess;

import "aabb";


public struct TescFactors {
    [mutating]
    public void set_zero() {
        tess_factors_[0] = 0;
        tess_factors_[1] = 0;
        tess_factors_[2] = 0;
        tess_factors_[3] = 0;
        inside_tess_factors_[0] = 0;
        inside_tess_factors_[1] = 0;
    }

    [mutating]
    public void interpolate_inside_factors() {
        inside_tess_factors_[0] = (tess_factors_[1] + tess_factors_[3]) * 0.5;
        inside_tess_factors_[1] = (tess_factors_[0] + tess_factors_[2]) * 0.5;
    }

    public float tess_factors_[4] : SV_TessFactor;
    public float inside_tess_factors_[2] : SV_InsideTessFactor;
};


float4 persp_mul(float4x4 m, float4 v) {
    var p = mul(m, v);
    p /= p.w;
    return p;
}


public float4 persp_mul(float4x4 m, float3 v) {
    return persp_mul(m, float4(v, 1));
}


public bool are_in_screen(float3[] points) {
    let screen_aabb = Aabb3D(float3(-1, -1, 0), float3(1, 1, 1));
    let points_aabb = Aabb3D(points);
    return screen_aabb.is_intersecting_cl(points_aabb);
}

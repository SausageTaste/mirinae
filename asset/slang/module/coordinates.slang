module coordinates;

import "aabb";


float4 persp_mul(float4x4 m, float4 v) {
    var p = mul(m, v);
    p /= p.w;
    return p;
}


public float4 persp_mul(float4x4 m, float3 v) {
    return persp_mul(m, float4(v, 1));
}


public bool is_is_screen(float2[] points) {
    let screen_aabb = Aabb2D(float2(-1, -1), float2(1, 1));
    let points_aabb = Aabb2D(points);
    return screen_aabb.is_intersecting_cl(points_aabb);
}

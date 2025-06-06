module lighting;


public float3x3 make_tbn_mat(float3 normal, float3 tangent, float3x3 m) {
    var T = normalize(mul(m, tangent));
    let N = normalize(mul(m, normal));
    T = normalize(T - dot(T, N) * N);
    let B = cross(N, T);
    return transpose(float3x3(T, B, N));
}

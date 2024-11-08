
mat3 make_tbn_mat(vec3 normal, vec3 tangent, mat4 m) {
    vec3 T = normalize(vec3(m * vec4(tangent, 0)));
    vec3 N = normalize(vec3(m * vec4(normal, 0)));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    return mat3(T, B, N);
}

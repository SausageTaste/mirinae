#version 450

#include "../utils/konst.glsl"


layout(location = 0) in vec3 v_local_pos;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_equirectangular_map;


float atan2(in float y, in float x) {
    bool s = (abs(x) > abs(y));
    return mix(PI/2.0 - atan(x,y), atan(y,x), s);
}


vec2 get_theta_phi(const vec3 v) {
    const float dv = sqrt(dot(v, v));
    const float x = v.x / dv;
    const float y = v.y / dv;
    const float z = v.z / dv;
    const float theta = atan2(z, x);
    const float phi = asin(-y);
    return vec2(theta, phi);
}


vec2 map_cube(const vec3 vec) {
    const vec2 theta_phi = get_theta_phi(vec);
    const float u = 0.5 + 0.5 * (theta_phi.x / PI);
    const float v = 0.5 + (theta_phi.y / PI);
    return vec2(u, v);
}


void main() {
    const vec3 normal = normalize(v_local_pos);
    const vec2 uv = map_cube(normal);
    f_color = texture(u_equirectangular_map, uv);
    f_color.w = 1.0;
}

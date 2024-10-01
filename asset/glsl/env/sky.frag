#version 450

#include "../utils/konst.glsl"


layout(location = 0) in vec3 v_local_pos;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_equirectangular_map;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 map_cube(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


void main() {
    const vec3 normal = normalize(v_local_pos);
    const vec2 uv = map_cube(normal);
    f_color = textureLod(u_equirectangular_map, uv, 0);
    f_color.w = 1.0;
}

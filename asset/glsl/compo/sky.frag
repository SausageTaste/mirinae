#version 450

#include "../utils/konst.glsl"

layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(push_constant) uniform U_CompoSkyMain {
    mat4 proj_inv;
    mat4 view_inv;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_sky_tex;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 map_cube(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


void main() {
    const vec4 clip_pos = vec4(v_uv_coord * 2 - 1, 1, 1);
    const vec4 frag_pos = u_pc.proj_inv * clip_pos;
    const vec3 view_direc = normalize(frag_pos.xyz / frag_pos.w);
    const vec3 world_direc = (u_pc.view_inv * vec4(view_direc, 0)).xyz;
    const vec2 uv = map_cube(normalize(world_direc));

    f_color = textureLod(u_sky_tex, uv, 0);
}

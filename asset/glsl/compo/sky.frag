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

    // Point light
    const vec3[] plight_poses = vec3[](
        vec3(23.063730118685, 7.4065945685438, -40.161457844117),
        vec3(-12.59026524203, 3.7424761322639, -57.913845097703),
        vec3(-27.187442727148, 2.8160032999329, -59.140437604074),
        vec3(-11.314874664971, 4.1150336202454, -72.997129503343),
        vec3(406.27578499388, 26.460298310523, -211.20135855825),
        vec3(17.723524030452, 4.5189922397826, -70.44753798105),
        vec3(-3.6262097794371, 0.75823541896961, -0.12247724517385)
    );
    const vec3[] plight_colors = vec3[](
        vec3(7, 24, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(24, 18, 7) * 0.5,
        vec3(240, 180, 70),
        vec3(7, 18, 24) * 2,
        vec3(1, 1, 1) * 5
    );

    for (uint i = 0; i < plight_poses.length(); ++i) {
        const vec3 light_pos = (inverse(u_pc.view_inv) * vec4(plight_poses[i], 1)).xyz;
        const vec3 cam_to_light = light_pos;
        const float projected_light_distance = dot(cam_to_light, view_direc);
        const vec3 projected_light_pos = projected_light_distance * view_direc;

        const float h = distance(light_pos, projected_light_pos);
        const float a = dot(-projected_light_pos, view_direc);
        const float b = dot((frag_pos.xyz / frag_pos.w) - projected_light_pos, view_direc);
        const float c = (atan(b / h) / h) - (atan(a / h) / h);
        f_color.xyz += plight_colors[i] * c * 0.01;
    }
}

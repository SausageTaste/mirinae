#version 450

#include "../utils/konst.glsl"
#include "../utils/lighting.glsl"

layout (location = 0) in vec2 v_texco;

layout (location = 0) out vec4 f_color;

layout (set = 0, binding = 0) uniform sampler2D u_depth_map;
layout (set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 0, binding = 2) uniform sampler2D u_normal_map;
layout (set = 0, binding = 3) uniform sampler2D u_material_map;

layout (push_constant) uniform U_CompoVplightPushConst {
    mat4 proj_inv;
    vec4 light_pos_v;
    vec4 light_color;
} u_pc;


void main() {
    const float depth_texel = texture(u_depth_map, v_texco).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_texco);
    const vec4 normal_texel = texture(u_normal_map, v_texco);
    const vec4 material_texel = texture(u_material_map, v_texco);

    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.y;
    const float metallic = material_texel.z;
    const vec3 frag_pos_v = calc_frag_pos(depth_texel, v_texco, u_pc.proj_inv);
    const vec3 view_to_frag_v = normalize(frag_pos_v);

    f_color = vec4(0, 0, 0, 1);

    {
        const vec3 light_pos_v = u_pc.light_pos_v.xyz;
        const vec3 view_to_light_v = light_pos_v;
        const float projected_light_distance = dot(view_to_light_v, view_to_frag_v);
        const vec3 projected_light_pos_v = projected_light_distance * view_to_frag_v;

        const float h = distance(light_pos_v, projected_light_pos_v);
        const float a = dot(-projected_light_pos_v, view_to_frag_v);
        const float b = dot(frag_pos_v - projected_light_pos_v, view_to_frag_v);
        const float c = (atan(b / h) / h) - (atan(a / h) / h);
        f_color.xyz += u_pc.light_color.xyz * c;
    }
}

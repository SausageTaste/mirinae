#version 450

#include "utils/lighting.glsl"

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec3 v_frag_pos;
layout(location = 4) in vec2 v_texcoord;

layout(location = 0) out vec4 out_composition;


layout(set = 0, binding = 0) uniform U_TransparentFrame {
    mat4 proj_inv;
    vec4 dlight_dir;
    vec4 dlight_color;
} u_comp_main;

layout(set = 1, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 1, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 1, binding = 2) uniform sampler2D u_normal_map;


void main() {
    vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    vec4 normal_texel = texture(u_normal_map, v_texcoord);

    const vec3 frag_pos = v_frag_pos;
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(v_tbn * (normal_texel.xyz * 2 - 1));
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;

    const vec3 view_direc = normalize(frag_pos);
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);

    vec3 light = albedo_texel.rgb * 0.2;

    // Directional light
    {
        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            u_comp_main.dlight_dir.xyz,
            1,
            u_comp_main.dlight_color.rgb
        );
    }

    // Flashlight
    {
        const vec3 light_pos = vec3(0, 0, 0);
        const vec3 to_light = normalize(light_pos - frag_pos);
        const vec3 light_dir = normalize(vec3(0, 0, -5) - light_pos);

        const float attenuation = calc_slight_attenuation(
            frag_pos,
            vec3(0),
            light_dir,
            cos(10 * 3.14159265 / 180),
            cos(25 * 3.14159265 / 180)
        );

        light += calc_pbr_illumination(
            roughness,
            metallic,
            albedo,
            normal,
            F0,
            -view_direc,
            to_light,
            frag_distance,
            vec3(3)
        ) * attenuation;
    }

    out_composition.rgb = light;
    out_composition.a = albedo_texel.a;
}

#version 450

#include "../utils/lighting.glsl"
#include "../utils/shadow.glsl"

layout (location = 0) in vec2 v_uv_coord;

layout (location = 0) out vec4 f_color;


layout (set = 0, binding = 0) uniform sampler2D u_depth_map;
layout (set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout (set = 0, binding = 2) uniform sampler2D u_normal_map;
layout (set = 0, binding = 3) uniform sampler2D u_material_map;

layout (set = 1, binding = 0) uniform samplerCube u_env_diffuse[1];
layout (set = 1, binding = 1) uniform samplerCube u_env_specular[1];
layout (set = 1, binding = 2) uniform sampler2D u_env_lut;

layout (push_constant) uniform U_CompoEnvmapPushConst {
    mat4 view_inv;
    mat4 proj_inv;
    vec4 fog_color_density;
} u_pc;


vec3 calc_frag_pos(float depth) {
    vec4 clip_pos = vec4(v_uv_coord * 2 - 1, depth, 1);
    vec4 frag_pos = u_pc.proj_inv * clip_pos;
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


vec3 ibl(
    const vec3 normal,
    const vec3 view_direc,
    const vec3 albedo,
    const vec3 f0,
    const float roughness,
    const float metallic,
    samplerCube env_diffuse,
    samplerCube env_specular
) {
    const vec3 N = normalize(normal);
    const vec3 V = -normalize(view_direc);
    const vec3 R = reflect(-V, N);
    const float NoV = max(dot(N, V), 0.0);
    const vec3 F = fresnel_schlick_rughness(NoV, f0, roughness);

    const vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const vec3 diffuse = texture(env_diffuse, N).rgb * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    const float mip_lvl = roughness * MAX_REFLECTION_LOD;
    const vec3 prefiltered_color = textureLod(env_specular, R, mip_lvl).rgb;
    const vec2 env_brdf = texture(u_env_lut, vec2(NoV, roughness)).rg;
    const vec3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);

    return kD * diffuse + specular;
}


void main() {
    const float depth_texel = texture(u_depth_map, v_uv_coord).r;
    const vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    const vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    const vec4 material_texel = texture(u_material_map, v_uv_coord);

    const vec3 frag_pos = calc_frag_pos(depth_texel);
    const vec3 albedo = albedo_texel.rgb;
    const vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.x;
    const float metallic = material_texel.y;

    const vec3 world_normal = (u_pc.view_inv * vec4(normal, 0)).xyz;
    const vec3 view_direc = normalize(frag_pos);
    const vec3 world_direc = (u_pc.view_inv * vec4(view_direc, 0)).xyz;
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);

    vec3 light = ibl(
        world_normal,
        world_direc,
        albedo,
        F0,
        roughness,
        metallic,
        u_env_diffuse[0],
        u_env_specular[0]
    );

    // Fog
    {
        const float x = frag_distance * u_pc.fog_color_density.w;
        const float xx = x * x;
        const float fog_factor = 1.0 / exp(xx);
        light = mix(u_pc.fog_color_density.xyz, light, fog_factor);
    }

    f_color = vec4(light, 1);
}

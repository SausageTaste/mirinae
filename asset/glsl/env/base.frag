#version 450

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec3 v_light;
layout(location = 4) in vec2 v_texcoord;

layout(location = 0) out vec4 out_albedo;


layout(set = 0, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;


void main() {
    vec4 albedo = texture(u_albedo_map, v_texcoord);
    vec4 normal_texel = texture(u_normal_map, v_texcoord);
    vec3 normal = v_tbn * (normal_texel.xyz * 2 - 1);

    out_albedo = vec4(albedo.rgb * v_light, 1);
}

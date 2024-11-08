#version 450

layout(location = 0) in mat3 v_tbn;
layout(location = 3) in vec2 v_texcoord;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;


layout(set = 0, binding = 0) uniform U_GbufModel {
    float roughness;
    float metallic;
} u_model;

layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;
layout(set = 0, binding = 3) uniform sampler2D u_orm_map;


void main() {
    vec4 albedo = texture(u_albedo_map, v_texcoord);
    vec4 normal_texel = texture(u_normal_map, v_texcoord);
    vec4 orm_texel = texture(u_orm_map, v_texcoord);
    vec3 normal = v_tbn * (normal_texel.xyz * 2 - 1);

    out_albedo = vec4(albedo.xyz, 1);
    out_normal = vec4(normalize(normal) * 0.5 + 0.5, 0);
    out_material = vec4(
        u_model.roughness * orm_texel.y,
        u_model.metallic * orm_texel.z,
        0,
        0
    );
}

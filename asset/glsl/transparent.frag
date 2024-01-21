#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 out_composition;


layout(set = 0, binding = 0) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo_texel = texture(u_albedo_map, v_texcoord);
    vec3 normal = normalize(v_normal);

    vec3 light_dir = normalize(vec3(0.5, 1, 0.5));
    float light = max(dot(normal, light_dir), 0) * 0.9 + 0.1;

    out_composition.rgb = albedo_texel.rgb * light;
    out_composition.a = albedo_texel.a;
}

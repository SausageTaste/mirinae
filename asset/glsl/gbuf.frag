#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;


layout(set = 0, binding = 0) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo = texture(u_albedo_map, v_texcoord);

    out_albedo = vec4(albedo.xyz, 1);
    out_normal = vec4(normalize(v_normal) * 0.5 + 0.5, 1);
    out_material = vec4(0.5, 0.0, 0.0, 0.0);
}

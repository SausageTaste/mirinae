#version 450


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform sampler2D u_depth_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;


void main() {
    vec4 depth_texel = texture(u_depth_map, v_uv_coord);
    vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    vec4 normal_texel = texture(u_normal_map, v_uv_coord);

    vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    vec3 light_dir = normalize(vec3(0.5, 1, 0.5));
    float light = max(dot(normal, light_dir), 0) * 0.9 + 0.1;

    f_color = vec4(albedo_texel.rgb * light, 1);
}

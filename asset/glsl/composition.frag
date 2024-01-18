#version 450


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform sampler2D u_depth_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;


void main() {
    vec4 depth = texture(u_depth_map, v_uv_coord);
    vec4 albedo = texture(u_albedo_map, v_uv_coord);
    vec4 normal = texture(u_normal_map, v_uv_coord);

    f_color = vec4(albedo.rgb, 1);
}

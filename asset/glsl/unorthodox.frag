#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo = texture(u_albedo_map, v_texcoord);

    float dlight_intensity = dot(v_normal, normalize(vec3(1, 1, 0)));
    dlight_intensity = smoothstep(0.0, 1.0, dlight_intensity) * 0.9 + 0.1;

    f_color = albedo;
    f_color.xyz *= dlight_intensity;
}

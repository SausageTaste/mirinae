#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 out_swapchain;
layout(location = 1) out vec4 out_albedo;
layout(location = 2) out vec4 out_normal;


layout(set = 0, binding = 0) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo = texture(u_albedo_map, v_texcoord);

    float dlight_intensity = dot(v_normal, normalize(vec3(1, 1, 0)));
    dlight_intensity = smoothstep(0.0, 1.0, dlight_intensity) * 0.9 + 0.1;

    out_albedo = albedo;
    out_normal = vec4(normalize(v_normal) * 0.5 + 0.5, 1);
    out_swapchain = albedo;
    out_swapchain.xyz *= dlight_intensity;
}

#version 450


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_depth_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;
layout(set = 0, binding = 2) uniform sampler2D u_normal_map;
layout(set = 0, binding = 3) uniform sampler2D u_material_map;

layout(set = 0, binding = 4) uniform U_CompositionMain {
    mat4 proj_inv;
    vec4 dlight_dir;
    vec4 dlight_color;
} u_comp_main;


vec3 calc_frag_pos(float depth) {
    vec4 clip_pos = vec4(v_uv_coord * 2 - 1, depth * 2 - 1, 1);
    vec4 frag_pos = u_comp_main.proj_inv * clip_pos;
    frag_pos /= frag_pos.w;
    return frag_pos.xyz;
}


void main() {
    float depth_texel = texture(u_depth_map, v_uv_coord).r;
    vec4 albedo_texel = texture(u_albedo_map, v_uv_coord);
    vec4 normal_texel = texture(u_normal_map, v_uv_coord);
    vec4 material_texel = texture(u_material_map, v_uv_coord);

    vec3 normal = normalize(normal_texel.xyz * 2 - 1);
    vec3 frag_pos = calc_frag_pos(depth_texel);
    float frag_distance = length(frag_pos);

    vec3 light = vec3(0.1);

    // Directional light
    {
        float cos_theta = max(dot(normal, u_comp_main.dlight_dir.xyz), 0);
        light += cos_theta * u_comp_main.dlight_color.rgb;
    }

    f_color = vec4(albedo_texel.rgb * light, 1);
}

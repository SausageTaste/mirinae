#version 450


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 1) uniform sampler2D u_color_map;
layout(set = 0, binding = 2) uniform sampler2D u_mask_map;


void main() {
    vec4 color_texel = texture(u_color_map, v_uv_coord);
    float mask_texel = texture(u_mask_map, v_uv_coord).r;

    f_color = color_texel;
    f_color.a *= mask_texel;
}

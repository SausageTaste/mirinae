#version 450

#include "utils/tone_mappings.glsl"


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform sampler2D u_compo_image;


void main() {
    vec4 compo_texel = texture(u_compo_image, v_uv_coord);
    vec3 mapped = aces_fitted(compo_texel.xyz);
    f_color = vec4(mapped.xyz * 0.1 + 0.9, 1);
}

#version 450

#include "utils/tone_mappings.glsl"


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform sampler2D u_compo_image;

layout(push_constant) uniform U_FillScreenPushConst {
    float exposure;
    float gamma;
} u_pc;


void main() {
    vec3 color = texture(u_compo_image, v_uv_coord).xyz;
    color = vec3(1.0) - exp(-color * u_pc.exposure);
    color = aces_approx(color);
    color = pow(color, vec3(1.0 / u_pc.gamma));
    f_color = vec4(color, 1);
}

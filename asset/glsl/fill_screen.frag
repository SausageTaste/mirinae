#version 450


layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform sampler2D u_composition_image;


void main() {
    vec4 composition_color = texture(u_composition_image, v_uv_coord);
    f_color = vec4(composition_color);
}

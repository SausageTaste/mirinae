#version 450

layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(set = 0, binding = 0) uniform sampler2D u_sky_tex;


void main() {
    f_color = texture(u_sky_tex, v_uv_coord);
}

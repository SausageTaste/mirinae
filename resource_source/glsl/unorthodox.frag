#version 450

layout(location = 0) in vec3 v_frag_color;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 f_color;


layout(binding = 1) uniform sampler2D u_albedo_map;


void main() {
    f_color = texture(u_albedo_map, v_texcoord);
}

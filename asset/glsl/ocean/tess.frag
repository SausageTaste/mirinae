#version 450

layout (location = 0) in vec3 i_normal;
layout (location = 1) in vec2 i_uv;

layout (location = 0) out vec4 f_color;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    float height_scale;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;


void main() {
    f_color = texture(u_height_map, i_uv);
}

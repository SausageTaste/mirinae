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
layout(set = 0, binding = 1) uniform sampler2D u_normal_map;


void main() {
    vec3 normal = textureLod(u_normal_map, i_uv, 0).xyz;
    normal = normalize(normal);
    vec3 light_dir = normalize(vec3(1, 1, 0));
    f_color.xyz = vec3(0.1) + vec3(0.9) * max(0, dot(normal, light_dir));
    //f_color = vec4(1);
}

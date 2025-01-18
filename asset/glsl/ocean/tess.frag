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
    vec3 normal = vec3(0);

    {
        const vec2 tile_size = vec2(30, 30);
        const vec2 size_per_texel = tile_size / u_pc.height_map_size_fbuf_size.xy;

        const float right = textureOffset(u_height_map, i_uv, ivec2( 2,  0)).r;
        const float left  = textureOffset(u_height_map, i_uv, ivec2(-2,  0)).r;
        const float up    = textureOffset(u_height_map, i_uv, ivec2( 0,  2)).r;
        const float down  = textureOffset(u_height_map, i_uv, ivec2( 0, -2)).r;

        normal = vec3(
            u_pc.height_scale * (left - right) / (size_per_texel.x * 4),
            1,
            u_pc.height_scale * (down - up) / (size_per_texel.y * 4)
        );
        normal = normalize(normal);
        normal = mat3(u_pc.view) * mat3(u_pc.model) * normal;
    }

    vec3 light_dir = normalize(mat3(u_pc.view) * vec3(1, 1, 0));
    f_color.xyz = vec3(0.1) + vec3(0.9) * max(0, dot(normal, light_dir));
}

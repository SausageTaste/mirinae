#version 450

layout (location = 0) in vec3 i_normal;
layout (location = 1) in vec2 i_uv;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;


layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size;
    float height_scale;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo_texel = texture(u_albedo_map, i_uv);
    out_albedo = vec4(albedo_texel.xyz, 1);
    out_material = vec4(0.9, 0, 0, 0);

    {
        const vec2 tile_size = vec2(60, 60);
        const vec2 terr_plane_size = tile_size * u_pc.tile_index_count.zw;
        const vec2 size_per_texel = terr_plane_size / u_pc.height_map_size.xy;

        const float right = textureOffset(u_height_map, i_uv, ivec2( 2,  0)).r;
        const float left  = textureOffset(u_height_map, i_uv, ivec2(-2,  0)).r;
        const float up    = textureOffset(u_height_map, i_uv, ivec2( 0,  2)).r;
        const float down  = textureOffset(u_height_map, i_uv, ivec2( 0, -2)).r;

        vec3 normal = vec3(
            u_pc.height_scale * (left - right) / (size_per_texel.x * 4),
            1,
            u_pc.height_scale * (down - up) / (size_per_texel.y * 4)
        );
        normal = normalize(normal);
        normal = mat3(u_pc.view) * mat3(u_pc.model) * normal;
        out_normal.xyz = normal * 0.5 + 0.5;
    }
}

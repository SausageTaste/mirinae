#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;


layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size;
    float height_scale;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;
layout(set = 0, binding = 1) uniform sampler2D u_albedo_map;


void main() {
    vec4 albedo_texel = texture(u_albedo_map, inUV);
    float height = texture(u_height_map, inUV).r;

    out_albedo = vec4(albedo_texel.xyz, 1);
    out_material = vec4(0.9, 0, 0, 0);

    {
        const vec2 tile_size = vec2(60, 60);
        const vec2 terr_plane_size = tile_size * u_pc.tile_index_count.zw;
        const vec2 size_per_texel = terr_plane_size / u_pc.height_map_size.xy;

        float right = u_pc.height_scale * textureOffset(u_height_map, inUV, ivec2( 1,  0)).r;
        float left  = u_pc.height_scale * textureOffset(u_height_map, inUV, ivec2(-1,  0)).r;
        float up    = u_pc.height_scale * textureOffset(u_height_map, inUV, ivec2( 0,  1)).r;
        float down  = u_pc.height_scale * textureOffset(u_height_map, inUV, ivec2( 0, -1)).r;
        vec3 normal = vec3(
            (left - right) / (size_per_texel.x + size_per_texel.x),
            1,
            (down - up) / (size_per_texel.y + size_per_texel.y)
        );
        normal = normalize(normal);
        normal = mat3(u_pc.view) * mat3(u_pc.model) * normal;
        out_normal.xyz = normal * 0.5 + 0.5;
    }
}

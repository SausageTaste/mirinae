#version 450

layout (location = 0) out vec2 v_uv;


layout (push_constant) uniform U_ShadowTerrainPushConst {
    mat4 pvm;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    vec4 terrain_size;
    float height_scale;
    float tess_factor;
} u_pc;


const vec2[] POSITIONS = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    const vec2 tile_size = u_pc.terrain_size.xy / u_pc.tile_index_count.zw;
    const vec2 tile_offset = u_pc.tile_index_count.xy * tile_size - u_pc.terrain_size.xy * 0.5;
    gl_Position.xz = POSITIONS[gl_VertexIndex] * tile_size + tile_offset;
    gl_Position.y = 0;
    gl_Position.w = 1;

    const vec2 uv_cell_size = vec2(1) / u_pc.tile_index_count.zw;
    const vec2 uv_start = uv_cell_size * u_pc.tile_index_count.xy;
    v_uv = uv_start + POSITIONS[gl_VertexIndex] * uv_cell_size;
}

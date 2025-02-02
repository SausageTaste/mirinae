#version 450

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec2 v_uv;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    vec2 texcoord_offset_rot[3];
    vec2 tile_dimensions;
} u_pc;


const vec3[] POSITIONS = vec3[4](
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(1, 0, 0)
);

const vec2 TEX_COORDS[] = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    const vec3 pos = vec3(
        TEX_COORDS[gl_VertexIndex].x * u_pc.tile_dimensions.x,
        0,
        TEX_COORDS[gl_VertexIndex].y * u_pc.tile_dimensions.y
    );
    const vec3 offset = vec3(
        u_pc.tile_index_count.x * u_pc.tile_dimensions.x,
        0,
        u_pc.tile_index_count.y * u_pc.tile_dimensions.y
    );

    gl_Position.xyz = pos + offset;
    gl_Position.w = 1;

    const vec2 uv_cell_size = vec2(
        1.0 / u_pc.tile_index_count.z,
        1.0 / u_pc.tile_index_count.w
    );
    const vec2 uv_start = uv_cell_size * u_pc.tile_index_count.xy;
    v_uv = TEX_COORDS[gl_VertexIndex];

    v_normal = vec3(0, 1, 0);
}

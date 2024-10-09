#version 450

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;


layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size;
    float height_scale;
} u_pc;


const float TILE_SIZE_X = 60.0;
const float TILE_SIZE_Y = 60.0;

const vec3[] POSITIONS = vec3[4](
    vec3(          0, 0,           0),
    vec3(          0, 0, TILE_SIZE_Y),
    vec3(TILE_SIZE_X, 0, TILE_SIZE_Y),
    vec3(TILE_SIZE_X, 0,           0)
);

const vec2 TEX_COORDS[] = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    vec3 offset = vec3(
        u_pc.tile_index_count.x * TILE_SIZE_X,
        0,
        u_pc.tile_index_count.y * TILE_SIZE_Y
    );

    vec2 uv_cell_size = vec2(
        1.0 / u_pc.tile_index_count.z,
        1.0 / u_pc.tile_index_count.w
    );
    vec2 uv_start = uv_cell_size * u_pc.tile_index_count.xy;
    vec2 uv = uv_start + TEX_COORDS[gl_VertexIndex] * uv_cell_size;

    gl_Position = vec4(POSITIONS[gl_VertexIndex] + offset, 1);
    outNormal = vec3(0, 1, 0);
    outUV = uv;
}

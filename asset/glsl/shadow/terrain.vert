#version 450

layout (location = 0) out vec2 v_uv;


layout (push_constant) uniform U_ShadowTerrainPushConst {
    mat4 pvm;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    float height_scale;
    float tess_factor;
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
    gl_Position.xyz = POSITIONS[gl_VertexIndex] + vec3(
        u_pc.tile_index_count.x * TILE_SIZE_X,
        0,
        u_pc.tile_index_count.y * TILE_SIZE_Y
    );
    gl_Position.w = 1;

    const vec2 uv_cell_size = vec2(
        1.0 / u_pc.tile_index_count.z,
        1.0 / u_pc.tile_index_count.w
    );
    const vec2 uv_start = uv_cell_size * u_pc.tile_index_count.xy;
    v_uv = uv_start + TEX_COORDS[gl_VertexIndex] * uv_cell_size;
}

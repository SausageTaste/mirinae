#version 450

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;


const float TILE_SIZE_X = 20.0;
const float TILE_SIZE_Y = 20.0;

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
    const int grid_size = int(ceil(sqrt(gl_InstanceIndex + 1)));
    const int grid_size_minus = grid_size - 1;
    const vec3 offset = vec3(
        TILE_SIZE_X * min(gl_InstanceIndex - grid_size_minus * grid_size_minus, grid_size_minus),
        0,
        TILE_SIZE_Y * min(-gl_InstanceIndex + grid_size * grid_size - 1, grid_size_minus)
    );

    gl_Position = vec4(POSITIONS[gl_VertexIndex] + offset, 1);
    outNormal = vec3(0, 1, 0);
    outUV = TEX_COORDS[gl_VertexIndex];
}

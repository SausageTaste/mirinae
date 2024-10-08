#version 450

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;


vec3[] POSITIONS = vec3[4](
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(1, 0, 0)
);

vec2 TEX_COORDS[] = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    const int grid_size = int(ceil(sqrt(gl_InstanceIndex + 1)));
    const int grid_size_minus = grid_size - 1;
    const vec3 offset = vec3(
        min(gl_InstanceIndex - grid_size_minus * grid_size_minus, grid_size_minus),
        0,
        min(-gl_InstanceIndex + grid_size * grid_size - 1, grid_size_minus)
    );

    gl_Position = vec4(POSITIONS[gl_VertexIndex] + offset, 1);
    outNormal = vec3(0, 1, 0);
    outUV = TEX_COORDS[gl_VertexIndex];
}

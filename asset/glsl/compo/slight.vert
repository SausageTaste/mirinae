#version 450

#include "../utils/fullscreen.glsl"

layout(location = 0) out vec2 v_uv_coord;


void main() {
    gl_Position = vec4(FULLSCREEN_POS[gl_VertexIndex], 0, 1);
    v_uv_coord = FULLSCREEN_UV[gl_VertexIndex];
}

#version 450

#include "../utils/fullscreen.glsl"

layout(location = 0) out vec2 v_texco;


void main() {
    gl_Position = vec4(FULLSCREEN_POS[gl_VertexIndex], 0, 1);
    v_texco = FULLSCREEN_UV[gl_VertexIndex];
}

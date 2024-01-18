#version 450


layout(location = 0) out vec2 v_uv_coord;

layout(set = 0, binding = 0) uniform U_OverlayMain {
    vec2 size;
    vec2 offset;
} u_overlay_main;


const vec2 POSITIONS[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    gl_Position = vec4(POSITIONS[gl_VertexIndex] * u_overlay_main.size + u_overlay_main.offset, 0, 1);
    v_uv_coord = POSITIONS[gl_VertexIndex];
}

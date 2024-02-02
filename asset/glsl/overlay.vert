#version 450


layout(location = 0) out vec2 v_uv_coord;

layout(set = 0, binding = 0) uniform U_OverlayMain {
    vec2 size;
    vec2 offset;
} u_overlay_main;

layout(push_constant) uniform U_OverlayPushConst {
    vec4 color;
    vec2 pos_offset;
    vec2 pos_scale;
    vec2 uv_offset;
    vec2 uv_scale;
} u_overlay_push_const;

const vec2 POSITIONS[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    gl_Position = vec4(POSITIONS[gl_VertexIndex] * u_overlay_push_const.pos_scale + u_overlay_push_const.pos_offset, 0, 1);
    v_uv_coord = POSITIONS[gl_VertexIndex] * u_overlay_push_const.uv_scale + u_overlay_push_const.uv_offset;
}

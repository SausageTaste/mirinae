#version 450


layout(location = 0) out vec2 v_uv_coord;


const vec2 POSITIONS[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    gl_Position = vec4(POSITIONS[gl_VertexIndex] * 0.2 + 0.2, 0, 1);
    v_uv_coord = POSITIONS[gl_VertexIndex];
}

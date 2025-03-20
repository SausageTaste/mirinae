#version 450


layout(location = 0) out vec2 v_uv_coord;


const vec2 POSITIONS[3] = vec2[](
    vec2(-1, -1),
    vec2(-1,  3),
    vec2( 3, -1)
);

const vec2 UV_COORDS[3] = vec2[](
    vec2(0, 0),
    vec2(0, 2),
    vec2(2, 0)
);


void main() {
    gl_Position = vec4(POSITIONS[gl_VertexIndex], 0, 1);
    gl_Position.z = 0;
    v_uv_coord = UV_COORDS[gl_VertexIndex];
}

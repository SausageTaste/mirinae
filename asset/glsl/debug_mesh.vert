#version 450


layout(push_constant) uniform U_DebugMeshPushConst {
    vec4 vertices[3];
    vec4 color;
} u_pc;


void main() {
    gl_Position = u_pc.vertices[gl_VertexIndex];
}

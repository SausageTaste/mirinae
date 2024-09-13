#version 450


layout(location = 0) out vec4 f_color;

layout(push_constant) uniform U_DebugMeshPushConst {
    vec4 vertices[3];
    vec4 color;
} u_pc;


void main() {
    f_color = u_pc.color;
}

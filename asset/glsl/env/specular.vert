#version 450

#include "vertices.glsl"


layout(location = 0) out vec3 v_local_pos;

layout(push_constant) uniform U_EnvSpecularPushConst {
    mat4 proj_view;
} u_push_const;


void main() {
    v_local_pos = CUBE_VERTICES[gl_VertexIndex];
    vec4 clip_pos = u_push_const.proj_view * vec4(v_local_pos, 0);
    gl_Position = clip_pos.xyww;
}

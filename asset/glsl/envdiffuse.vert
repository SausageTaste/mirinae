#version 450

layout (location = 0) in vec3 i_position;

layout(location = 0) out vec3 v_local_pos;

layout(push_constant) uniform U_EnvdiffusePushConst {
    mat4 proj_view;
} u_push_const;


const vec3 VERTICES[36] = vec3[](
    vec3(-1,  1, -1),
    vec3(-1, -1, -1),
    vec3( 1, -1, -1),
    vec3( 1, -1, -1),
    vec3( 1,  1, -1),
    vec3(-1,  1, -1),

    vec3(-1, -1,  1),
    vec3(-1, -1, -1),
    vec3(-1,  1, -1),
    vec3(-1,  1, -1),
    vec3(-1,  1,  1),
    vec3(-1, -1,  1),

    vec3( 1, -1, -1),
    vec3( 1, -1,  1),
    vec3( 1,  1,  1),
    vec3( 1,  1,  1),
    vec3( 1,  1, -1),
    vec3( 1, -1, -1),

    vec3(-1, -1,  1),
    vec3(-1,  1,  1),
    vec3( 1,  1,  1),
    vec3( 1,  1,  1),
    vec3( 1, -1,  1),
    vec3(-1, -1,  1),

    vec3(-1,  1, -1),
    vec3( 1,  1, -1),
    vec3( 1,  1,  1),
    vec3( 1,  1,  1),
    vec3(-1,  1,  1),
    vec3(-1,  1, -1),

    vec3(-1, -1, -1),
    vec3(-1, -1,  1),
    vec3( 1, -1, -1),
    vec3( 1, -1, -1),
    vec3(-1, -1,  1),
    vec3( 1, -1,  1)
);


void main() {
    v_local_pos = VERTICES[gl_VertexIndex];
    vec4 clip_pos = u_push_const.proj_view * vec4(v_local_pos, 1);
    gl_Position = clip_pos.xyww;
}

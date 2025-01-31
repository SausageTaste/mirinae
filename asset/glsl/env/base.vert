#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;

layout(location = 0) out mat3 v_tbn;
layout(location = 3) out vec3 v_light;
layout(location = 4) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufActor {
    mat4 model;
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;

layout(push_constant) uniform U_EnvmapPushConst {
    mat4 proj_view;
    vec4 dlight_dir;
    vec4 dlight_color;
} u_push_const;


void main() {
    gl_Position = u_push_const.proj_view * u_gbuf_model.model * vec4(i_pos, 1);

    v_light = vec3(0.2) ;

    vec3 normal = normalize(i_normal);
    float light_align = max(dot(normal, u_push_const.dlight_dir.xyz), 0);
    v_light += light_align * u_push_const.dlight_color.xyz * 0.8;

    v_texcoord = i_texcoord;
}

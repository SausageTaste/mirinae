#version 450

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texcoord;
layout(location = 3) in vec4 i_jweights;
layout(location = 4) in ivec4 i_jids;

layout(location = 0) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufActorSkinned {
    mat4 joint_transforms[128];
    mat4 view_model;
    mat4 pvm;
} u_gbuf_model;


mat4 make_joint_transform() {
    if (i_jids[0] < 0) {
        return mat4(1);
    }

    mat4 joint_transform = mat4(0);
    for (int i = 0; i < 4; i++) {
        if (i_jids[i] < 0)
            break;
        else
            joint_transform += i_jweights[i] * u_gbuf_model.joint_transforms[i_jids[i]];
    }

    return joint_transform;
}


void main() {
    mat4 joint_mat = make_joint_transform();

    gl_Position = u_gbuf_model.pvm * joint_mat * vec4(i_pos, 1);
    v_normal = i_normal;
    v_texcoord = i_texcoord;
}

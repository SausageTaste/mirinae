#version 450

const int MAX_JOINTS = 256;

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec3 i_tangent;
layout(location = 3) in vec2 i_texcoord;
layout(location = 4) in vec4 i_jweights;
layout(location = 5) in ivec4 i_jids;

layout(location = 0) out mat3 v_tbn;
layout(location = 3) out vec2 v_texcoord;


layout(set = 1, binding = 0) uniform U_GbufActorSkinned {
    mat4 joint_transforms[MAX_JOINTS];
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
            joint_transform += i_jweights[i]
                                * u_gbuf_model.joint_transforms[i_jids[i]];
    }

    return joint_transform;
}


void main() {
    mat4 joint_mat = make_joint_transform();

    gl_Position = u_gbuf_model.pvm * joint_mat * vec4(i_pos, 1);
    v_tbn = mat3(u_gbuf_model.view_model * joint_mat) * mat3(i_tangent, normalize(cross(i_normal, i_tangent)), i_normal);
    v_texcoord = i_texcoord;
}

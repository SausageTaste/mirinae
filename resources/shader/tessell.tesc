#version 450 core

layout (vertices = 4) out;

in vec2 v_uv[];
out vec2 tc_uv[];

uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;

const int MIN_TESS_LEVEL = 1;
const int MAX_TESS_LEVEL = 64;
const float MIN_DISTANCE = 5;
const float MAX_DISTANCE = 300;


int calc_outer_lvl(vec3 p0, vec3 p1) {
    float distance00 = clamp((length(p0)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);
    float distance01 = clamp((length(p1)-MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0);
    float tessLevel = mix(MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance00, distance01));
    return int(tessLevel);
}


void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tc_uv[gl_InvocationID] = v_uv[gl_InvocationID];

    if (gl_InvocationID == 0) {
        vec4 view_pos_00 = u_view_mat * u_model_mat * gl_in[0].gl_Position;
        vec4 view_pos_10 = u_view_mat * u_model_mat * gl_in[1].gl_Position;
        vec4 view_pos_11 = u_view_mat * u_model_mat * gl_in[2].gl_Position;
        vec4 view_pos_01 = u_view_mat * u_model_mat * gl_in[3].gl_Position;

        gl_TessLevelOuter[1] = calc_outer_lvl(view_pos_00.xyz, view_pos_10.xyz);
        gl_TessLevelOuter[2] = calc_outer_lvl(view_pos_10.xyz, view_pos_11.xyz);
        gl_TessLevelOuter[3] = calc_outer_lvl(view_pos_11.xyz, view_pos_01.xyz);
        gl_TessLevelOuter[0] = calc_outer_lvl(view_pos_01.xyz, view_pos_00.xyz);

        gl_TessLevelInner[0] = (gl_TessLevelOuter[1] + gl_TessLevelOuter[3]) * 0.5;
        gl_TessLevelInner[1] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[2]) * 0.5;
    }
}

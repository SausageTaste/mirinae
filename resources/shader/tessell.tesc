#version 450 core

layout (vertices = 4) out;

in vec2 v_uv[];
out vec2 tc_uv[];

uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;


void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tc_uv[gl_InvocationID] = v_uv[gl_InvocationID];

    if (gl_InvocationID == 0) {
        const int MIN_TESS_LVL = 1;
        const int MAX_TESS_LVL = 64;

        vec4 ndc_pos_00 = u_proj_mat * u_view_mat * u_model_mat * gl_in[0].gl_Position;
        vec4 ndc_pos_10 = u_proj_mat * u_view_mat * u_model_mat * gl_in[1].gl_Position;
        vec4 ndc_pos_11 = u_proj_mat * u_view_mat * u_model_mat * gl_in[2].gl_Position;
        vec4 ndc_pos_01 = u_proj_mat * u_view_mat * u_model_mat * gl_in[3].gl_Position;

        ndc_pos_00 /= ndc_pos_00.w;
        ndc_pos_10 /= ndc_pos_10.w;
        ndc_pos_11 /= ndc_pos_11.w;
        ndc_pos_01 /= ndc_pos_01.w;

        gl_TessLevelOuter[1] = clamp(distance(ndc_pos_00.xy, ndc_pos_10.xy) * 10, MIN_TESS_LVL, MAX_TESS_LVL);
        gl_TessLevelOuter[2] = clamp(distance(ndc_pos_10.xy, ndc_pos_11.xy) * 10, MIN_TESS_LVL, MAX_TESS_LVL);
        gl_TessLevelOuter[3] = clamp(distance(ndc_pos_11.xy, ndc_pos_01.xy) * 10, MIN_TESS_LVL, MAX_TESS_LVL);
        gl_TessLevelOuter[0] = clamp(distance(ndc_pos_01.xy, ndc_pos_00.xy) * 10, MIN_TESS_LVL, MAX_TESS_LVL);

        gl_TessLevelInner[0] = (gl_TessLevelOuter[1] + gl_TessLevelOuter[3]) * 0.5;
        gl_TessLevelInner[1] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[2]) * 0.5;
    }
}

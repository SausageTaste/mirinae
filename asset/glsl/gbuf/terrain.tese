#version 450

layout (quads, fractional_odd_spacing, cw) in;

layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 projection;
    mat4 view;
    mat4 model;
    float tessAlpha;
    float tessLevel;
} u_main;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main(void) {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p11 = gl_in[2].gl_Position;
    vec4 p10 = gl_in[3].gl_Position;

    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    gl_Position = u_main.projection * u_main.view * u_main.model * p;

    outNormal = vec3(0, 1, 0);
    outUV = vec2(0, 0);
}

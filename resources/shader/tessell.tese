#version 450 core

layout (quads, fractional_odd_spacing, ccw) in;

in vec2 tc_uv[];

out float te_height;

uniform sampler2D u_height_map;
uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;


void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // ----------------------------------------------------------------------
    // retrieve control point texture coordinates
    vec2 t00 = tc_uv[0];
    vec2 t01 = tc_uv[1];
    vec2 t10 = tc_uv[2];
    vec2 t11 = tc_uv[3];

    // bilinearly interpolate texture coordinate across patch
    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 tex_coord = (t1 - t0) * v + t0;

    // lookup texel at patch coordinate for height and scale + shift as desired
    te_height = texture(u_height_map, tex_coord).y * 64.0 - 16.0;

    // ----------------------------------------------------------------------
    // retrieve control point position coordinates
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    // compute patch surface normal
    vec4 u_vec = p01 - p00;
    vec4 v_vec = p10 - p00;
    vec4 normal = normalize( vec4(cross(v_vec.xyz, u_vec.xyz), 0) );

    // bilinearly interpolate position coordinate across patch
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    // displace point along normal
    p += normal * te_height;

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = u_proj_mat * u_view_mat * u_model_mat * p;
}

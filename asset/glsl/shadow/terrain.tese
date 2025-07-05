#version 450

layout (quads, equal_spacing, cw) in;

layout (location = 0) in vec2 i_uv[];


layout (push_constant) uniform U_ShadowTerrainPushConst {
    mat4 pvm_;
    vec2 fbuf_size_;
    float height_scale_;
    float tess_factor_;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;


void main() {
    const float u = gl_TessCoord.x;
    const float v = gl_TessCoord.y;

    const vec2 t00 = i_uv[0];
    const vec2 t01 = i_uv[1];
    const vec2 t11 = i_uv[2];
    const vec2 t10 = i_uv[3];

    const vec2 t0 = (t01 - t00) * u + t00;
    const vec2 t1 = (t11 - t10) * u + t10;
    const vec2 tex_coord = (t1 - t0) * v + t0;

    const float height = texture(u_height_map, tex_coord).r * u_pc.height_scale_;

    const vec3 p00 = gl_in[0].gl_Position.xyz;
    const vec3 p01 = gl_in[1].gl_Position.xyz;
    const vec3 p11 = gl_in[2].gl_Position.xyz;
    const vec3 p10 = gl_in[3].gl_Position.xyz;

    const vec3 p0 = (p01 - p00) * u + p00;
    const vec3 p1 = (p11 - p10) * u + p10;
    vec3 p = (p1 - p0) * v + p0;
    p.y += height;
    gl_Position = u_pc.pvm_ * vec4(p, 1);
}

#version 450

layout (quads, equal_spacing, cw) in;

layout (location = 0) in vec3 i_normal[];
layout (location = 1) in vec2 i_uv[];

layout (location = 0) out vec3 o_frag_pos;
layout (location = 1) out vec2 o_uv;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    vec2 tile_dimensions;
    float height_scale;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_height_map[3];


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

    const vec3 p00 = gl_in[0].gl_Position.xyz;
    const vec3 p01 = gl_in[1].gl_Position.xyz;
    const vec3 p11 = gl_in[2].gl_Position.xyz;
    const vec3 p10 = gl_in[3].gl_Position.xyz;

    const vec3 p0 = (p01 - p00) * u + p00;
    const vec3 p1 = (p11 - p10) * u + p10;
    vec3 p = (p1 - p0) * v + p0;

    for (int i = 0; i < 3; ++i)
        p.xyz += texture(u_height_map[i], tex_coord).xyz;

    o_frag_pos = (u_pc.view * u_pc.model * vec4(p, 1)).xyz;
    gl_Position = u_pc.pvm * vec4(p, 1);

    o_uv = tex_coord;
}

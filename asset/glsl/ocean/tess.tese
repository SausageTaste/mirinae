#version 450

#include "../utils/complex.glsl"

layout (quads, equal_spacing, cw) in;

layout (location = 0) in vec3 i_normal[];
layout (location = 1) in vec2 i_uv[];

layout (location = 0) out vec4 o_lod_scales;
layout (location = 1) out vec3 o_frag_pos;
layout (location = 2) out vec2 o_uv;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 patch_offset_scale;
    vec4 tile_dims_n_fbuf_size;
    vec4 tile_index_count;
} u_pc;

layout (set = 0, binding = 0) uniform U_OceanTessParams {
    vec4 texco_offset_rot_[3];
    vec4 fog_color_density;
    vec4 jacobian_scale;
    vec4 len_scales_lod_scale;
    float foam_bias;
    float foam_scale;
    float foam_threshold;
    float sss_base;
    float sss_scale;
} u_params;

layout (set = 0, binding = 1) uniform sampler2D u_disp_map[3];


vec2 transform_uv(vec2 uv, int cascade) {
    vec2 tile_idx = u_pc.tile_index_count.xy;
    vec2 offset = u_params.texco_offset_rot_[cascade].xy;
    vec2 scale = u_params.texco_offset_rot_[cascade].zw;

    const vec2 global_uv = complex_init(uv);
    const vec2 offset_rot = complex_init(scale);
    return complex_mul(global_uv, offset_rot) + offset;
}


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

    float p_dist = length((u_pc.view * u_pc.model * vec4(p, 1)).xyz);
    float lod_c0 = min(u_params.len_scales_lod_scale[3] * u_params.len_scales_lod_scale[0] / p_dist, 1);
    float lod_c1 = min(u_params.len_scales_lod_scale[3] * u_params.len_scales_lod_scale[1] / p_dist, 1);
    float lod_c2 = min(u_params.len_scales_lod_scale[3] * u_params.len_scales_lod_scale[2] / p_dist, 1);

    vec3 displacement = vec3(0);
    float largeWavesBias = 0;
    displacement += texture(u_disp_map[0], transform_uv(tex_coord, 0)).xyz * lod_c0;
    largeWavesBias = displacement.y;
    displacement += texture(u_disp_map[1], transform_uv(tex_coord, 1)).xyz * lod_c1;
    displacement += texture(u_disp_map[2], transform_uv(tex_coord, 2)).xyz * lod_c2;
    p += displacement;
    o_lod_scales = vec4(lod_c0, lod_c1, lod_c2, max(displacement.y - largeWavesBias * 0.8 - u_params.sss_base, 0) / u_params.sss_scale);

    o_frag_pos = (u_pc.view * u_pc.model * vec4(p, 1)).xyz;
    gl_Position = u_pc.pvm * vec4(p, 1);

    o_uv = tex_coord;
}

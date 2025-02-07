#version 450

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec2 v_uv;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
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


const vec3[] POSITIONS = vec3[4](
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(1, 0, 0)
);

const vec2 TEX_COORDS[] = vec2[4](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(1, 0)
);


void main() {
    const vec2 tile_dim = u_pc.tile_dims_n_fbuf_size.xy;
    const vec3 pos = vec3(
        TEX_COORDS[gl_VertexIndex].x * tile_dim.x,
        0,
        TEX_COORDS[gl_VertexIndex].y * tile_dim.y
    );
    const vec3 offset = vec3(
        u_pc.tile_index_count.x * tile_dim.x,
        0,
        u_pc.tile_index_count.y * tile_dim.y
    );

    gl_Position.xyz = pos + offset;
    gl_Position.w = 1;

    const vec2 uv_cell_size = vec2(
        1.0 / u_pc.tile_index_count.z,
        1.0 / u_pc.tile_index_count.w
    );
    const vec2 uv_start = uv_cell_size * u_pc.tile_index_count.xy;
    v_uv = TEX_COORDS[gl_VertexIndex];

    v_normal = vec3(0, 1, 0);
}

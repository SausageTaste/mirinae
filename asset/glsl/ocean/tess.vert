#version 450

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec2 v_uv;


layout (push_constant) uniform U_OceanTessPushConst {
    mat4 pvm;
    mat4 view;
    mat4 model;
    vec4 patch_offset_scale;
    vec4 tile_dims_n_fbuf_size;
    vec4 tile_index_count;
    float patch_height;
} u_pc;

layout (set = 0, binding = 0) uniform U_OceanTessParams {
    vec4 texco_offset_rot_[3];
    vec4 dlight_color;
    vec4 dlight_dir;
    vec4 fog_color_density;
    vec4 jacobian_scale;
    vec4 len_scales_lod_scale;
    vec4 ocean_color;
    float foam_bias;
    float foam_scale;
    float foam_threshold;
    float roughness;
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
    const vec2 offset = u_pc.patch_offset_scale.xy;
    const vec2 scale = u_pc.patch_offset_scale.zw;
    const vec2 patch_pos = TEX_COORDS[gl_VertexIndex] * scale + offset;
    gl_Position.xyz = vec3(patch_pos.x, u_pc.patch_height, patch_pos.y);
    gl_Position.w = 1;
    v_uv = patch_pos;
    v_normal = vec3(0, 1, 0);
}

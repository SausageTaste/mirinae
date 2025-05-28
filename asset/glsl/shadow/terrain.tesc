#version 450

layout (vertices = 4) out;
layout (location = 0) in vec2 v_uv[];
layout (location = 0) out vec2 o_uv[4];


layout (push_constant) uniform U_ShadowTerrainPushConst {
    mat4 pvm;
    vec4 tile_index_count;
    vec4 height_map_size_fbuf_size;
    vec4 terrain_size;
    float height_scale;
    float tess_factor;
} u_pc;


void main() {
    const float MAX_TESS_LEVEL = 64;

    if (gl_InvocationID == 0) {
        vec4 p00 = u_pc.pvm * gl_in[0].gl_Position;
        vec4 p01 = u_pc.pvm * gl_in[1].gl_Position;
        vec4 p11 = u_pc.pvm * gl_in[2].gl_Position;
        vec4 p10 = u_pc.pvm * gl_in[3].gl_Position;
        p00 /= p00.w;
        p01 /= p01.w;
        p11 /= p11.w;
        p10 /= p10.w;

        const vec2 fbuf_size = u_pc.height_map_size_fbuf_size.zw;
        p00.xy = (p00.xy * 0.5 + 0.5) * fbuf_size;
        p01.xy = (p01.xy * 0.5 + 0.5) * fbuf_size;
        p11.xy = (p11.xy * 0.5 + 0.5) * fbuf_size;
        p10.xy = (p10.xy * 0.5 + 0.5) * fbuf_size;

        float edge0 = distance(p00.xy, p01.xy);
        float edge1 = distance(p01.xy, p11.xy);
        float edge2 = distance(p11.xy, p10.xy);
        float edge3 = distance(p10.xy, p00.xy);

        float tess_level0 = min(edge3 * u_pc.tess_factor, MAX_TESS_LEVEL);
        float tess_level1 = min(edge0 * u_pc.tess_factor, MAX_TESS_LEVEL);
        float tess_level2 = min(edge1 * u_pc.tess_factor, MAX_TESS_LEVEL);
        float tess_level3 = min(edge2 * u_pc.tess_factor, MAX_TESS_LEVEL);

        gl_TessLevelOuter[0] = tess_level0;
        gl_TessLevelOuter[1] = tess_level1;
        gl_TessLevelOuter[2] = tess_level2;
        gl_TessLevelOuter[3] = tess_level3;

        gl_TessLevelInner[0] = (tess_level1 + tess_level3) * 0.5;
        gl_TessLevelInner[1] = (tess_level0 + tess_level2) * 0.5;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    o_uv[gl_InvocationID] = v_uv[gl_InvocationID];
}

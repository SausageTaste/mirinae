#version 450

layout (quads, equal_spacing, cw) in;

layout (push_constant) uniform U_GbufTerrainPushConst {
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 tile_index_count;
    vec4 height_map_size;
    float height_scale;
} u_main;

layout(set = 0, binding = 0) uniform sampler2D u_height_map;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main(void) {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec2 t00 = inUV[0];
    vec2 t01 = inUV[1];
    vec2 t11 = inUV[2];
    vec2 t10 = inUV[3];

    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 texCoord = (t1 - t0) * v + t0;

    float height = texture(u_height_map, texCoord).r * u_main.height_scale;

    vec3 p00 = gl_in[0].gl_Position.xyz;
    vec3 p01 = gl_in[1].gl_Position.xyz;
    vec3 p11 = gl_in[2].gl_Position.xyz;
    vec3 p10 = gl_in[3].gl_Position.xyz;

    vec3 p0 = (p01 - p00) * u + p00;
    vec3 p1 = (p11 - p10) * u + p10;
    vec3 p = (p1 - p0) * v + p0;
    p.y += height;
    gl_Position = u_main.projection * u_main.view * u_main.model * vec4(p, 1);

    const vec3 normal = mat3(u_main.view) * mat3(u_main.model) * vec3(0, 1, 0);
    outNormal = normalize(normal) * 0.5 + 0.5;

    outUV = texCoord;
}

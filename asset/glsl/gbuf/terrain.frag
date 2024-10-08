#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;


void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(vec3(-4.0, -4.0, 0.0));

    out_albedo = vec4(1, 0, 0, 1);
    out_normal.xyz = inNormal;
    out_material = vec4(0.5, 0.5, 0.0, 0.0);
}

#version 450

#include "../utils/konst.glsl"


layout(location = 0) in vec3 v_local_pos;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform samplerCube u_envmap;


void main() {
    const vec3 normal = normalize(v_local_pos);
    const vec3 right = cross(vec3(0, 1, 0), normal);
    const vec3 up = cross(normal, right);

    vec3 irradiance = vec3(0);
    int num_samples = 0;
    const float sample_delta = 0.1;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
            const vec3 tangent_sample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            const vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal;
            irradiance += textureLod(u_envmap, sample_vec, 0.0).rgb * cos(theta) * sin(theta);
            num_samples++;
        }
    }

    f_color.xyz = PI * irradiance * (1.0 / float(num_samples));
    f_color.w = 1;
}

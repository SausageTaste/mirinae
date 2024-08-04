#version 450

layout(location = 0) in vec3 v_local_pos;

layout(location = 0) out vec4 f_color;

layout(set = 0, binding = 0) uniform samplerCube u_envmap;


const float PI = 3.14159265359;


vec3 convolute() {
    vec3 normal     = normalize(v_local_pos);
    vec3 irradiance = vec3(0);
    vec3 up         = vec3(0, 1, 0);
    vec3 right      = cross(up, normal);

    up = cross(normal, right);

    float sampleDelta = 0.1;
    int numSamples = 0;
    for ( float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta ) {
        for ( float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta ) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += textureLod(u_envmap, sampleVec, 0.0).rgb * cos(theta) * sin(theta);
            numSamples++;
        }
    }

    return PI * irradiance * (1.0 / float(numSamples));
}


void main() {
    f_color = vec4(convolute(), 1.0);
}

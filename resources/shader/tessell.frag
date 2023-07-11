#version 410 core

in vec3 te_normal;
in float te_height;

out vec4 FragColor;


void main() {
    float light = dot(te_normal, normalize(vec3(2, 5, 5)));
    float h = (te_height + 16)/64.0;
    FragColor = vec4(vec3(light), 1.0);
}

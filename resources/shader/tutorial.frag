#version 450 core

in vec3 v_normal;
in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;


void main() {
    float light = dot(v_normal, normalize(vec3(2, 5, 1)));
    vec3 color = texture(texture2, v_uv).xyz * light;
    FragColor = vec4(color, 1);
}

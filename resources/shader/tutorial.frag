#version 450 core

in vec3 v_normal;
in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;


void main() {
    FragColor = mix(vec4(0), texture(texture2, v_uv), v_normal.x);
}

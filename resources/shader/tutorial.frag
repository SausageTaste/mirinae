#version 450 core

in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;


void main() {
    FragColor = mix(texture(texture1, v_uv), texture(texture2, v_uv), 0.5);
}

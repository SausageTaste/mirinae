#version 450 core

in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D texture1;


void main() {
    FragColor = texture(texture1, v_uv);
}

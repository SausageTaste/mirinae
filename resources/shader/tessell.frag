#version 410 core

in float te_height;

out vec4 FragColor;


void main() {
    float h = (te_height + 16)/64.0f;
    FragColor = vec4(h, h, h, 1.0);
}

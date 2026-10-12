#version 450

layout(location = 0) in vec3 rail_color;
layout(location = 0) out vec4 color;

void main() {
    color = vec4(rail_color, 1.0);
}

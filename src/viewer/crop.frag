#version 450

layout(location = 0) in vec2 circle_position;
layout(location = 0) out vec4 color;

void main() {
    if (dot(circle_position, circle_position) > 1.0) discard;
    color = vec4(0.18, 0.78, 0.28, 1.0);
}

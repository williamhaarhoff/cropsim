#version 450

layout(location = 0) in vec2 ellipse_position;
layout(location = 1) in vec3 leaf_color;
layout(location = 0) out vec4 color;

void main() {
    if (dot(ellipse_position, ellipse_position) > 1.0) discard;
    color = vec4(leaf_color, 1.0);
}

#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 vertex_color;
layout(location = 0) out vec3 rail_color;
layout(set = 1, binding = 0) uniform Camera { vec2 half_extents; } camera;

void main() {
    rail_color = vertex_color;
    gl_Position = vec4(position / camera.half_extents, -0.1, 1.0);
}

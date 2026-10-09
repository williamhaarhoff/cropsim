#version 450

layout(location = 0) in vec2 instance_position;
layout(location = 1) in float instance_radius;
layout(location = 0) out vec2 circle_position;
layout(set = 1, binding = 0) uniform Camera { vec2 half_extents; } camera;

const vec2 corners[6] = vec2[6](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main() {
    circle_position = corners[gl_VertexIndex];
    vec2 world_position = instance_position + circle_position * instance_radius;
    gl_Position = vec4(world_position / camera.half_extents, 0.0, 1.0);
}

#version 450

layout(location = 0) in vec2 instance_position;
layout(location = 1) in vec2 instance_radii;
layout(location = 2) in vec2 instance_rotation;
layout(location = 3) in vec3 instance_color;
layout(location = 0) out vec2 ellipse_position;
layout(location = 1) out vec3 leaf_color;
layout(set = 1, binding = 0) uniform Camera { vec2 half_extents; } camera;

const vec2 corners[6] = vec2[6](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main() {
    ellipse_position = corners[gl_VertexIndex];
    leaf_color = instance_color;
    vec2 local = ellipse_position * instance_radii;
    mat2 rotation = mat2(instance_rotation.x, instance_rotation.y,
                         -instance_rotation.y, instance_rotation.x);
    vec2 world_position = instance_position + rotation * local;
    gl_Position = vec4(world_position / camera.half_extents, 0.0, 1.0);
}

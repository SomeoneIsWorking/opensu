#version 450

// The sampled image as it is: the overlay, mixed over the target by its pipeline's blend, and a
// shader preset's output drawn below full resolution.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;

layout(set = 0, binding = 0) uniform sampler2D image;

void main() {
    colour = texture(image, uv);
}

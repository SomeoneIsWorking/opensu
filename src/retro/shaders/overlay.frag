#version 450

// The runner's overlay, the size of the target, mixed over it by the pipeline's blend.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;

layout(set = 0, binding = 0) uniform sampler2D overlay;

void main() {
    colour = texture(overlay, uv);
}

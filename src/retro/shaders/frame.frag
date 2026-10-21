#version 450
#extension GL_GOOGLE_include_directive : require

#include "frame_grid.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D frame;

void main() {
    color = vec4(gridShaded(texture(frame, uv).rgb, uv * push.texels), 1.0);
}

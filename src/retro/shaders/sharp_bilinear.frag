#version 450
#extension GL_GOOGLE_include_directive : require

// Sharp bilinear: each texel stays flat over the largest whole multiple of it that fits, and only
// the fraction left over at its edges is blended. Drawn with the linear sampler.
#include "frame_grid.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D frame;

void main() {
    vec2 texel = uv * push.texels;
    vec2 within = fract(texel) - 0.5;
    vec2 plateau = 0.5 - 0.5 / push.prescale;
    vec2 blend = (within - clamp(within, -plateau, plateau)) * push.prescale + 0.5;
    vec3 sampled = texture(frame, (floor(texel) + blend) / push.texels).rgb;
    color = vec4(gridShaded(sampled, texel), 1.0);
}

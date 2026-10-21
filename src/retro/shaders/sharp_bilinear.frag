#version 450

// Sharp bilinear: each texel stays flat over the largest whole multiple of it that fits, and only
// the fraction left over at its edges is blended. Drawn with the linear sampler.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D frame;
layout(push_constant) uniform Scale {
    vec2 texels;
    // The whole factor per axis, at least 1.
    vec2 prescale;
} scale;

void main() {
    vec2 texel = uv * scale.texels;
    vec2 within = fract(texel) - 0.5;
    vec2 plateau = 0.5 - 0.5 / scale.prescale;
    vec2 blend = (within - clamp(within, -plateau, plateau)) * scale.prescale + 0.5;
    color = vec4(texture(frame, (floor(texel) + blend) / scale.texels).rgb, 1.0);
}

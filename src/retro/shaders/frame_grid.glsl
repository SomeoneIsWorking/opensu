// The push block the nearest, bilinear and sharp-bilinear shaders read, and the grid filters they
// draw over the frame: a dark band at the right and bottom edge of every source pixel.
layout(push_constant) uniform Push {
    // The frame texture's size.
    vec2 texels;
    // Sharp bilinear's whole factor per axis, at least 1.
    vec2 prescale;
    // The band's width across and down, in source pixels; 0 draws none.
    vec2 band;
    // How much of the light under a band it takes.
    float shade;
} push;

// How much of [from, to] (in source pixels) lies in the bands [n + 1 - width, n + 1).
float bandCover(float from, float to, float width) {
    float start = 1.0 - width;
    float below = floor(from) * width + max(fract(from) - start, 0.0);
    float through = floor(to) * width + max(fract(to) - start, 0.0);
    return (through - below) / max(to - from, 1e-6);
}

// `colour` darkened by the share of this output pixel's footprint on `texel` that the bands cover,
// so the bands stay on source pixel edges at any scale.
vec3 gridShaded(vec3 colour, vec2 texel) {
    vec2 footprint = fwidth(texel);
    vec2 from = texel - 0.5 * footprint;
    vec2 to = texel + 0.5 * footprint;
    float across = bandCover(from.x, to.x, push.band.x);
    float down = bandCover(from.y, to.y, push.band.y);
    return colour * (1.0 - push.shade * (across + down - across * down));
}

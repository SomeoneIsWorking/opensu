// The push constants every flow pass shares (FlowPush in flow_interpolator.cpp).
layout(push_constant) uniform Push {
    ivec2 size;   // the level's or the frame's pixels
    ivec2 blocks; // the motion field's blocks
    int range;    // the coarsest level's search radius, in its pixels
    int hasParent;
    float t;
    // The content's pixel in the frame's, 1 x 1 when not known to be scaled.
    float pixelWidth;
    float pixelHeight;
    int scale; // the real and made frames' pixel in the pixels flow works on
} push;

// Motion blocks are this many pixels square at every level (`flowBlockPixels`).
const int blockSize = 8;

// While measuring, every texel a pass reads from an image and every value it reads from shared
// memory is counted (`FlowInterpolator::submit` reads them).
layout(constant_id = 0) const bool countReads = false;
layout(set = 3, binding = 0) buffer Reads {
    uint texels;
    uint sharedReads;
} reads;

void countRead() {
    if (countReads) {
        atomicAdd(reads.texels, 1u);
    }
}

void countShared() {
    if (countReads) {
        atomicAdd(reads.sharedReads, 1u);
    }
}

#define READ(image, p) (countRead(), imageLoad(image, p))
#define SHARED(value) (countShared(), value)

// Where a frame between two real ones takes the earlier one's pixel for a motion d from the
// earlier to the later, at time t: pixel art moves by whole content pixels, so the share counts d
// in them, rounded toward zero, and a step of one content pixel over two frames shows the earlier
// frame's place rather than a half step. A zoom or a turn, off the content's grid, shows the
// earlier frame's scale or angle until the share reaches a whole pixel.
ivec2 earlierShare(vec2 d, float t) {
    const vec2 pixel = vec2(push.pixelWidth, push.pixelHeight);
    const vec2 steps = round(d / pixel);
    return ivec2(round(trunc(steps * t + sign(steps) * 1e-3) * pixel));
}

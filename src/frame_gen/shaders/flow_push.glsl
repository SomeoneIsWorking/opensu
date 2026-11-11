// The push constants every flow pass shares (FlowPush in flow_interpolator.cpp).
layout(push_constant) uniform Push {
    ivec2 size;   // the level's or the frame's pixels
    ivec2 blocks; // the motion field's blocks
    int range;    // the coarsest level's search radius, in its pixels
    int hasParent;
    float t;
    float unused;
} push;

// Motion blocks are this many pixels square at every level.
const int blockSize = 8;

// Where a frame between two real ones takes the earlier one's pixel for a motion d from the
// earlier to the later, at time t: rounded to whole pixels, so pixel art stays sharp.
ivec2 earlierShare(vec2 d, float t) {
    return ivec2(floor(d * t + 0.5));
}

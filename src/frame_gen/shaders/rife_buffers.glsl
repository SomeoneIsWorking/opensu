// The RIFE graph's storage (`rife_net.cpp`): every blob in one buffer as planes of rows, every
// weight, bias and constant in another.
layout(set = 0, binding = 0) buffer Blobs {
    float blobs[];
};
layout(set = 0, binding = 1) readonly buffer Weights {
    float weights[];
};

// The value of the blob at `offset` of `w` by `h` at (x, y) of plane `c`.
float blobAt(int offset, int w, int h, int x, int y, int c) {
    return blobs[offset + (c * h + y) * w + x];
}

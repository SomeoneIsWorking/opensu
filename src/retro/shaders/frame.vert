#version 450

// One triangle covering the viewport, which the presenter sets to the letterboxed frame.
layout(location = 0) out vec2 uv;

// The picture a quarter turn clockwise.
layout(constant_id = 0) const bool turned = false;

void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    uv = turned ? vec2(corner.y, 1.0 - corner.x) : corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}

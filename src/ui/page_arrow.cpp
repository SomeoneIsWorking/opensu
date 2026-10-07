#include "page_arrow.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "raylib.h"

namespace iideck::ui {
namespace {

// iiSU nx2.o: shadow #66000000 dropped 0.11 width.
constexpr Color shadow{0, 0, 0, 0x66};
// iiSU vi5.q #2A3140 at alpha 186 (dark), vi5.r #EEF4FA at 221 (light).
constexpr Color darkFill{0x2A, 0x31, 0x40, 186};
constexpr Color lightFill{0xEE, 0xF4, 0xFA, 221};
constexpr Color darkStroke{0xF4, 0xF7, 0xFB, 0xC8};
constexpr Color lightStroke{0x33, 0x40, 0x52, 0xAA};
constexpr Color darkHighlight{0xFF, 0xFF, 0xFF, 0x88};
constexpr Color lightHighlight{0xFF, 0xFF, 0xFF, 0xCC};
constexpr int cornerSteps = 6;

Vector2 towards(Vector2 from, Vector2 to, float distance) noexcept {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::max(std::hypot(dx, dy), 0.001f);
    return Vector2{from.x + dx / length * distance, from.y + dy / length * distance};
}

/// iiSU nx2.a: each corner cut back along both edges and bridged by a quadratic curve.
std::vector<Vector2> roundedTriangle(const Rect& rect, bool pointsLeft) {
    const float base = rect.x + rect.width * (pointsLeft ? 0.8f : 0.2f);
    const float tip = rect.x + rect.width * (pointsLeft ? 0.2f : 0.8f);
    const std::array<Vector2, 3> corners{Vector2{base, rect.y + rect.height * 0.12f},
                                         Vector2{tip, rect.centreY()},
                                         Vector2{base, rect.y + rect.height * 0.88f}};
    const float radius = std::min(rect.width, rect.height) * 0.18f;
    std::vector<Vector2> outline;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const Vector2 corner = corners[i];
        const Vector2 before = corners[(i + corners.size() - 1) % corners.size()];
        const Vector2 after = corners[(i + 1) % corners.size()];
        const float cut =
            std::min({radius, std::hypot(before.x - corner.x, before.y - corner.y) * 0.5f,
                      std::hypot(after.x - corner.x, after.y - corner.y) * 0.5f});
        const Vector2 start = towards(corner, before, cut);
        const Vector2 end = towards(corner, after, cut);
        for (int step = 0; step <= cornerSteps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(cornerSteps);
            const float u = 1.0f - t;
            outline.push_back(Vector2{u * u * start.x + 2.0f * u * t * corner.x + t * t * end.x,
                                      u * u * start.y + 2.0f * u * t * corner.y + t * t * end.y});
        }
    }
    return outline;
}

/// raylib wants counter-clockwise on screen, which is a negative cross product with y down.
void triangle(Vector2 a, Vector2 b, Vector2 c, Color colour) {
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) {
        std::swap(b, c);
    }
    DrawTriangle(a, b, c, colour);
}

std::vector<Vector2> shifted(std::vector<Vector2> points, float dy) {
    for (Vector2& point : points) {
        point.y += dy;
    }
    return points;
}

void fill(const std::vector<Vector2>& outline, Color colour) {
    // The outline is convex; a fan from its centroid covers it.
    Vector2 centre{};
    for (const Vector2& point : outline) {
        centre.x += point.x;
        centre.y += point.y;
    }
    centre.x /= static_cast<float>(outline.size());
    centre.y /= static_cast<float>(outline.size());
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const Vector2 a = outline[i];
        const Vector2 b = outline[(i + 1) % outline.size()];
        triangle(centre, a, b, colour);
    }
}

void stroke(const std::vector<Vector2>& outline, float width, Color colour) {
    // A closed band of quads offset along each vertex's averaged normal.
    const std::size_t count = outline.size();
    std::vector<Vector2> inner(count);
    std::vector<Vector2> outer(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Vector2 previous = outline[(i + count - 1) % count];
        const Vector2 next = outline[(i + 1) % count];
        const float dx = next.x - previous.x;
        const float dy = next.y - previous.y;
        const float length = std::max(std::hypot(dx, dy), 0.001f);
        const Vector2 normal{-dy / length * width * 0.5f, dx / length * width * 0.5f};
        inner[i] = Vector2{outline[i].x - normal.x, outline[i].y - normal.y};
        outer[i] = Vector2{outline[i].x + normal.x, outline[i].y + normal.y};
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t j = (i + 1) % count;
        triangle(inner[i], outer[i], outer[j], colour);
        triangle(inner[i], outer[j], inner[j], colour);
    }
}

void paintArrow(const Rect& rect, bool pointsLeft, bool dark) {
    const std::vector<Vector2> outline = roundedTriangle(rect, pointsLeft);
    fill(shifted(outline, rect.width * 0.11f), shadow);
    fill(outline, dark ? darkFill : lightFill);
    stroke(outline, std::clamp(rect.width * 0.058f, 1.9f, 2.8f), dark ? darkStroke : lightStroke);
    stroke(shifted(outline, -std::clamp(rect.width * 0.033f, 1.0f, 1.6f)),
           std::clamp(rect.width * 0.023f, 0.8f, 1.1f), dark ? darkHighlight : lightHighlight);
}

} // namespace

void PageArrowPainter::paint(const PageArrows& arrows, bool dark) const {
    if (arrows.previous) {
        paintArrow(*arrows.previous, true, dark);
    }
    if (arrows.next) {
        paintArrow(*arrows.next, false, dark);
    }
}

} // namespace iideck::ui

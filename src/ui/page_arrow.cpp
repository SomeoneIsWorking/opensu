#include "page_arrow.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace opensu::ui {
namespace {

// iiSU nx2.o: shadow #66000000 dropped 0.11 width.
constexpr Colour shadow{0, 0, 0, 0x66};
// iiSU vi5.q #2A3140 at alpha 186 (dark), vi5.r #EEF4FA at 221 (light).
constexpr Colour darkFill{0x2A, 0x31, 0x40, 186};
constexpr Colour lightFill{0xEE, 0xF4, 0xFA, 221};
constexpr Colour darkStroke{0xF4, 0xF7, 0xFB, 0xC8};
constexpr Colour lightStroke{0x33, 0x40, 0x52, 0xAA};
constexpr Colour darkHighlight{0xFF, 0xFF, 0xFF, 0x88};
constexpr Colour lightHighlight{0xFF, 0xFF, 0xFF, 0xCC};
constexpr int cornerSteps = 6;

Point towards(Point from, Point to, float distance) noexcept {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::max(std::hypot(dx, dy), 0.001f);
    return Point{from.x + dx / length * distance, from.y + dy / length * distance};
}

/// iiSU nx2.a: each corner cut back along both edges and bridged by a quadratic curve.
std::vector<Point> roundedTriangle(const Rect& rect, bool pointsLeft) {
    const float base = rect.x + rect.width * (pointsLeft ? 0.8f : 0.2f);
    const float tip = rect.x + rect.width * (pointsLeft ? 0.2f : 0.8f);
    const std::array<Point, 3> corners{Point{base, rect.y + rect.height * 0.12f},
                                       Point{tip, rect.centreY()},
                                       Point{base, rect.y + rect.height * 0.88f}};
    const float radius = std::min(rect.width, rect.height) * 0.18f;
    std::vector<Point> outline;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const Point corner = corners[i];
        const Point before = corners[(i + corners.size() - 1) % corners.size()];
        const Point after = corners[(i + 1) % corners.size()];
        const float cut =
            std::min({radius, std::hypot(before.x - corner.x, before.y - corner.y) * 0.5f,
                      std::hypot(after.x - corner.x, after.y - corner.y) * 0.5f});
        const Point start = towards(corner, before, cut);
        const Point end = towards(corner, after, cut);
        for (int step = 0; step <= cornerSteps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(cornerSteps);
            const float u = 1.0f - t;
            outline.push_back(Point{u * u * start.x + 2.0f * u * t * corner.x + t * t * end.x,
                                    u * u * start.y + 2.0f * u * t * corner.y + t * t * end.y});
        }
    }
    return outline;
}

std::vector<Point> shifted(std::vector<Point> points, float dy) {
    for (Point& point : points) {
        point.y += dy;
    }
    return points;
}

void fill(Canvas& canvas, const std::vector<Point>& outline, Colour colour) {
    // The outline is convex; a fan from its centroid covers it.
    Point centre{};
    for (const Point& point : outline) {
        centre.x += point.x;
        centre.y += point.y;
    }
    centre.x /= static_cast<float>(outline.size());
    centre.y /= static_cast<float>(outline.size());
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const Point a = outline[i];
        const Point b = outline[(i + 1) % outline.size()];
        canvas.triangle(centre, a, b, colour);
    }
}

void stroke(Canvas& canvas, const std::vector<Point>& outline, float width, Colour colour) {
    // A closed band of quads offset along each vertex's averaged normal.
    const std::size_t count = outline.size();
    std::vector<Point> inner(count);
    std::vector<Point> outer(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Point previous = outline[(i + count - 1) % count];
        const Point next = outline[(i + 1) % count];
        const float dx = next.x - previous.x;
        const float dy = next.y - previous.y;
        const float length = std::max(std::hypot(dx, dy), 0.001f);
        const Point normal{-dy / length * width * 0.5f, dx / length * width * 0.5f};
        inner[i] = Point{outline[i].x - normal.x, outline[i].y - normal.y};
        outer[i] = Point{outline[i].x + normal.x, outline[i].y + normal.y};
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t j = (i + 1) % count;
        canvas.triangle(inner[i], outer[i], outer[j], colour);
        canvas.triangle(inner[i], outer[j], inner[j], colour);
    }
}

void paintArrow(Canvas& canvas, const Rect& rect, bool pointsLeft, bool dark) {
    const std::vector<Point> outline = roundedTriangle(rect, pointsLeft);
    fill(canvas, shifted(outline, rect.width * 0.11f), shadow);
    fill(canvas, outline, dark ? darkFill : lightFill);
    stroke(canvas, outline, std::clamp(rect.width * 0.058f, 1.9f, 2.8f),
           dark ? darkStroke : lightStroke);
    stroke(canvas, shifted(outline, -std::clamp(rect.width * 0.033f, 1.0f, 1.6f)),
           std::clamp(rect.width * 0.023f, 0.8f, 1.1f), dark ? darkHighlight : lightHighlight);
}

} // namespace

void PageArrowPainter::paint(Canvas& canvas, const PageArrows& arrows, bool dark) const {
    if (arrows.previous) {
        paintArrow(canvas, *arrows.previous, true, dark);
    }
    if (arrows.next) {
        paintArrow(canvas, *arrows.next, false, dark);
    }
}

} // namespace opensu::ui

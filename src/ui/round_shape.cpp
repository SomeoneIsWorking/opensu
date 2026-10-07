#include "round_shape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "rlgl.h"

namespace iideck::ui {
namespace {

/// Points around a rounded rectangle, clockwise on screen from the top-left arc. Every
/// rectangle cut the same way yields the same count, so two outlines pair point for point.
std::vector<Vector2> outline(const RoundRect& shape, Tessellation cut) {
    const Rect& r = shape.rect;
    const float radius = std::clamp(shape.radius, 0.0f, std::min(r.width, r.height) * 0.5f);
    const std::array<Vector2, 4> centres{
        Vector2{r.x + radius, r.y + radius},
        Vector2{r.right() - radius, r.y + radius},
        Vector2{r.right() - radius, r.bottom() - radius},
        Vector2{r.x + radius, r.bottom() - radius},
    };
    const int arcs = std::max(cut.cornerSegments, 1);
    const int edges = std::max(cut.edgeSegments, 1);
    std::vector<Vector2> points;
    points.reserve(static_cast<std::size_t>(4 * (arcs + edges)));
    for (std::size_t corner = 0; corner < centres.size(); ++corner) {
        // Arcs run from 180, 270, 0 and 90 degrees, a quarter turn each.
        const float start = std::numbers::pi_v<float> * (1.0f + 0.5f * static_cast<float>(corner));
        for (int step = 0; step <= arcs; ++step) {
            const float angle = start + std::numbers::pi_v<float> * 0.5f *
                                            static_cast<float>(step) / static_cast<float>(arcs);
            points.push_back(Vector2{centres[corner].x + radius * std::cos(angle),
                                     centres[corner].y + radius * std::sin(angle)});
        }
        const Vector2 from = points.back();
        const float next = start + std::numbers::pi_v<float> * 0.5f;
        const Vector2& nextCentre = centres[(corner + 1) % centres.size()];
        const Vector2 to{nextCentre.x + radius * std::cos(next),
                         nextCentre.y + radius * std::sin(next)};
        for (int step = 1; step < edges; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(edges);
            points.push_back(Vector2{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
        }
    }
    return points;
}

/// Untextured triangles sample raylib's 1x1 white texture; rlSetTexture(0) leaves the last
/// texture bound when the draw mode does not change.
void bindWhite() {
    rlSetTexture(rlGetTextureIdDefault());
}

void vertex(Vector2 point, Color colour) {
    rlColor4ub(colour.r, colour.g, colour.b, colour.a);
    rlVertex2f(point.x, point.y);
}

/// One triangle in the winding raylib keeps with back-face culling on.
void triangle(Vector2 a, Color ca, Vector2 b, Color cb, Vector2 c, Color cc) {
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    vertex(a, ca);
    if (area > 0.0f) {
        vertex(c, cc);
        vertex(b, cb);
        return;
    }
    vertex(b, cb);
    vertex(c, cc);
}

std::uint8_t channel(float value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

RoundRect RoundRect::grown(float by) const noexcept {
    return RoundRect{Rect{rect.x - by, rect.y - by, std::max(rect.width + by * 2.0f, 0.0f),
                          std::max(rect.height + by * 2.0f, 0.0f)},
                     std::max(radius + by, 0.0f)};
}

Color withAlpha(Color colour, float alpha) noexcept {
    colour.a = channel(static_cast<float>(colour.a) * std::clamp(alpha, 0.0f, 1.0f));
    return colour;
}

float luminance(Color colour) noexcept {
    const auto linear = [](unsigned char channel) {
        const float c = static_cast<float>(channel) / 255.0f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * linear(colour.r) + 0.7152f * linear(colour.g) + 0.0722f * linear(colour.b);
}

Color mix(Color from, Color to, float t) noexcept {
    const float c = std::clamp(t, 0.0f, 1.0f);
    const auto lerp = [c](std::uint8_t a, std::uint8_t b) {
        return channel(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * c);
    };
    return Color{lerp(from.r, to.r), lerp(from.g, to.g), lerp(from.b, to.b), lerp(from.a, to.a)};
}

void fillRoundRect(const RoundRect& shape, const VertexColour& colour, Tessellation cut) {
    if (shape.rect.width <= 0.0f || shape.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Vector2> points = outline(shape, cut);
    const Vector2 centre{shape.rect.centreX(), shape.rect.centreY()};
    const Color middle = colour(centre, 0.0f);
    bindWhite();
    rlBegin(RL_TRIANGLES);
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vector2 a = points[i];
        const Vector2 b = points[(i + 1) % points.size()];
        triangle(centre, middle, a, colour(a, 1.0f), b, colour(b, 1.0f));
    }
    rlEnd();
}

void fillBand(const RoundRect& outer, const RoundRect& inner, const VertexColour& colour,
              Tessellation cut) {
    if (outer.rect.width <= 0.0f || outer.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Vector2> out = outline(outer, cut);
    const std::vector<Vector2> in = outline(inner, cut);
    bindWhite();
    rlBegin(RL_TRIANGLES);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const std::size_t j = (i + 1) % out.size();
        const Color o0 = colour(out[i], 1.0f);
        const Color o1 = colour(out[j], 1.0f);
        const Color i0 = colour(in[i], 0.0f);
        const Color i1 = colour(in[j], 0.0f);
        triangle(out[i], o0, in[i], i0, in[j], i1);
        triangle(out[i], o0, in[j], i1, out[j], o1);
    }
    rlEnd();
}

void strokeSoft(const RoundRect& centre, float width, float blur, const RoundRect& clip,
                const VertexColour& colour, Tessellation cut) {
    const float half = width * 0.5f;
    // A blurred edge reaches half alpha at the stroke's edge and none at blur beyond it.
    const auto alphaAt = [half, blur](float distance) {
        if (blur <= 0.0f) {
            return std::abs(distance) <= half ? 1.0f : 0.0f;
        }
        return std::clamp((half + blur - std::abs(distance)) / (2.0f * blur), 0.0f, 1.0f);
    };
    const float reach = half + std::max(blur, 0.0f);
    const float solid = std::max(half - std::max(blur, 0.0f), 0.0f);
    // The clip is concentric with the stroke, so clipping caps how far outward it reaches.
    const float room = std::max(centre.rect.x - clip.rect.x, 0.0f);
    std::vector<float> knots{-reach, -solid, solid, reach};
    if (blur <= 0.0f) {
        knots = {-half, half};
    }
    for (float& knot : knots) {
        knot = std::min(knot, room);
    }
    for (std::size_t i = 0; i + 1 < knots.size(); ++i) {
        const float inner = knots[i];
        const float outer = knots[i + 1];
        if (outer <= inner) {
            continue;
        }
        const float innerAlpha = blur <= 0.0f ? 1.0f : alphaAt(inner);
        const float outerAlpha = blur <= 0.0f ? 1.0f : alphaAt(outer);
        fillBand(
            centre.grown(outer), centre.grown(inner),
            [&colour, innerAlpha, outerAlpha](Vector2 point, float across) {
                return withAlpha(colour(point, across),
                                 innerAlpha + (outerAlpha - innerAlpha) * across);
            },
            cut);
    }
}

void fillSoft(const RoundRect& shape, float blur, const VertexColour& colour, Tessellation cut) {
    if (blur <= 0.0f) {
        fillRoundRect(shape, colour, cut);
        return;
    }
    fillRoundRect(shape.grown(-blur), colour, cut);
    fillBand(
        shape.grown(blur), shape.grown(-blur),
        [&colour](Vector2 point, float across) {
            return withAlpha(colour(point, across), 1.0f - across);
        },
        cut);
}

void drawTextureRound(const RoundRect& shape, const Texture& texture, Rectangle source,
                      Color tint) {
    if (texture.id == 0 || shape.rect.width <= 0.0f || shape.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Vector2> points = outline(shape, Tessellation{});
    const auto width = static_cast<float>(texture.width);
    const auto height = static_cast<float>(texture.height);
    const auto uv = [&](Vector2 point) {
        const float u = (point.x - shape.rect.x) / shape.rect.width;
        const float v = (point.y - shape.rect.y) / shape.rect.height;
        return Vector2{(source.x + u * source.width) / width,
                       (source.y + v * source.height) / height};
    };
    const Vector2 centre{shape.rect.centreX(), shape.rect.centreY()};
    rlSetTexture(texture.id);
    rlBegin(RL_TRIANGLES);
    const auto put = [&](Vector2 point) {
        const Vector2 coord = uv(point);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(coord.x, coord.y);
        rlVertex2f(point.x, point.y);
    };
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vector2 a = points[i];
        const Vector2 b = points[(i + 1) % points.size()];
        const float area =
            (a.x - centre.x) * (b.y - centre.y) - (a.y - centre.y) * (b.x - centre.x);
        put(centre);
        put(area > 0.0f ? b : a);
        put(area > 0.0f ? a : b);
    }
    rlEnd();
    bindWhite();
}

} // namespace iideck::ui

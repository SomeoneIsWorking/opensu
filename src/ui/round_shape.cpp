#include "round_shape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace opensu::ui {
namespace {

/// Points around a rounded rectangle, clockwise on screen from the top-left arc. Every
/// rectangle cut the same way yields the same count, so two outlines pair point for point.
std::vector<Point> outline(const RoundRect& shape, Tessellation cut) {
    const Rect& r = shape.rect;
    const float radius = std::clamp(shape.radius, 0.0f, std::min(r.width, r.height) * 0.5f);
    const std::array<Point, 4> centres{
        Point{r.x + radius, r.y + radius},
        Point{r.right() - radius, r.y + radius},
        Point{r.right() - radius, r.bottom() - radius},
        Point{r.x + radius, r.bottom() - radius},
    };
    const int arcs = std::max(cut.cornerSegments, 1);
    const int edges = std::max(cut.edgeSegments, 1);
    std::vector<Point> points;
    points.reserve(4 * static_cast<std::size_t>(arcs + edges));
    for (std::size_t corner = 0; corner < centres.size(); ++corner) {
        // Arcs run from 180, 270, 0 and 90 degrees, a quarter turn each.
        const float start = std::numbers::pi_v<float> * (1.0f + 0.5f * static_cast<float>(corner));
        for (int step = 0; step <= arcs; ++step) {
            const float angle = start + std::numbers::pi_v<float> * 0.5f *
                                            static_cast<float>(step) / static_cast<float>(arcs);
            points.push_back(Point{centres[corner].x + radius * std::cos(angle),
                                   centres[corner].y + radius * std::sin(angle)});
        }
        const Point from = points.back();
        const float next = start + std::numbers::pi_v<float> * 0.5f;
        const Point& nextCentre = centres[(corner + 1) % centres.size()];
        const Point to{nextCentre.x + radius * std::cos(next),
                       nextCentre.y + radius * std::sin(next)};
        for (int step = 1; step < edges; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(edges);
            points.push_back(Point{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
        }
    }
    return points;
}

/// Triangles of coloured vertices, drawn in one call.
class Mesh {
  public:
    explicit Mesh(std::size_t triangles) {
        vertices_.reserve(triangles * 3);
        indices_.reserve(triangles * 3);
    }

    void triangle(Point a, Colour ca, Point b, Colour cb, Point c, Colour cc) {
        for (const auto& [point, colour] : {std::pair{a, ca}, std::pair{b, cb}, std::pair{c, cc}}) {
            indices_.push_back(static_cast<std::uint32_t>(vertices_.size()));
            vertices_.push_back(gpu2d::Vertex{.x = point.x,
                                              .y = point.y,
                                              .u = 0.0f,
                                              .v = 0.0f,
                                              .r = colour.r,
                                              .g = colour.g,
                                              .b = colour.b,
                                              .a = colour.a});
        }
    }

    void draw(Canvas& canvas) const {
        canvas.mesh(gpu2d::TextureView{}, vertices_, indices_);
    }

  private:
    std::vector<gpu2d::Vertex> vertices_;
    std::vector<std::uint32_t> indices_;
};

std::uint8_t channel(float value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

RoundRect RoundRect::grown(float by) const noexcept {
    return RoundRect{Rect{rect.x - by, rect.y - by, std::max(rect.width + by * 2.0f, 0.0f),
                          std::max(rect.height + by * 2.0f, 0.0f)},
                     std::max(radius + by, 0.0f)};
}

Colour withAlpha(Colour colour, float alpha) noexcept {
    colour.a = channel(static_cast<float>(colour.a) * std::clamp(alpha, 0.0f, 1.0f));
    return colour;
}

float luminance(Colour colour) noexcept {
    const auto linear = [](std::uint8_t channel) {
        const float c = static_cast<float>(channel) / 255.0f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * linear(colour.r) + 0.7152f * linear(colour.g) + 0.0722f * linear(colour.b);
}

Colour mix(Colour from, Colour to, float t) noexcept {
    const float c = std::clamp(t, 0.0f, 1.0f);
    const auto lerp = [c](std::uint8_t a, std::uint8_t b) {
        return channel(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * c);
    };
    return Colour{lerp(from.r, to.r), lerp(from.g, to.g), lerp(from.b, to.b), lerp(from.a, to.a)};
}

void fillRoundRect(Canvas& canvas, const RoundRect& shape, const VertexColour& colour,
                   Tessellation cut) {
    if (shape.rect.width <= 0.0f || shape.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Point> points = outline(shape, cut);
    const Point centre{shape.rect.centreX(), shape.rect.centreY()};
    const Colour middle = colour(centre, 0.0f);
    Mesh mesh{points.size()};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point a = points[i];
        const Point b = points[(i + 1) % points.size()];
        mesh.triangle(centre, middle, a, colour(a, 1.0f), b, colour(b, 1.0f));
    }
    mesh.draw(canvas);
}

void fillBand(Canvas& canvas, const RoundRect& outer, const RoundRect& inner,
              const VertexColour& colour, Tessellation cut) {
    if (outer.rect.width <= 0.0f || outer.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Point> out = outline(outer, cut);
    const std::vector<Point> in = outline(inner, cut);
    Mesh mesh{out.size() * 2};
    for (std::size_t i = 0; i < out.size(); ++i) {
        const std::size_t j = (i + 1) % out.size();
        const Colour o0 = colour(out[i], 1.0f);
        const Colour o1 = colour(out[j], 1.0f);
        const Colour i0 = colour(in[i], 0.0f);
        const Colour i1 = colour(in[j], 0.0f);
        mesh.triangle(out[i], o0, in[i], i0, in[j], i1);
        mesh.triangle(out[i], o0, in[j], i1, out[j], o1);
    }
    mesh.draw(canvas);
}

void strokeSoft(Canvas& canvas, const RoundRect& centre, float width, float blur,
                const RoundRect& clip, const VertexColour& colour, Tessellation cut) {
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
            canvas, centre.grown(outer), centre.grown(inner),
            [&colour, innerAlpha, outerAlpha](Point point, float across) {
                return withAlpha(colour(point, across),
                                 innerAlpha + (outerAlpha - innerAlpha) * across);
            },
            cut);
    }
}

void fillSoft(Canvas& canvas, const RoundRect& shape, float blur, const VertexColour& colour,
              Tessellation cut) {
    if (blur <= 0.0f) {
        fillRoundRect(canvas, shape, colour, cut);
        return;
    }
    fillRoundRect(canvas, shape.grown(-blur), colour, cut);
    fillBand(
        canvas, shape.grown(blur), shape.grown(-blur),
        [&colour](Point point, float across) {
            return withAlpha(colour(point, across), 1.0f - across);
        },
        cut);
}

void drawTextureRound(Canvas& canvas, const RoundRect& shape, const gpu2d::TextureView& texture,
                      const gpu2d::Rect& source, Colour tint) {
    if (texture.texture == nullptr || texture.width <= 0 || texture.height <= 0 ||
        shape.rect.width <= 0.0f || shape.rect.height <= 0.0f) {
        return;
    }
    const std::vector<Point> points = outline(shape, Tessellation{});
    const auto width = static_cast<float>(texture.width);
    const auto height = static_cast<float>(texture.height);
    const auto vertex = [&](Point point) {
        const float u = (point.x - shape.rect.x) / shape.rect.width;
        const float v = (point.y - shape.rect.y) / shape.rect.height;
        return gpu2d::Vertex{.x = point.x,
                             .y = point.y,
                             .u = (source.x + u * source.width) / width,
                             .v = (source.y + v * source.height) / height,
                             .r = tint.r,
                             .g = tint.g,
                             .b = tint.b,
                             .a = tint.a};
    };
    std::vector<gpu2d::Vertex> vertices;
    vertices.reserve(points.size() + 1);
    vertices.push_back(vertex(Point{shape.rect.centreX(), shape.rect.centreY()}));
    for (const Point& point : points) {
        vertices.push_back(vertex(point));
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(points.size() * 3);
    const auto count = static_cast<std::uint32_t>(points.size());
    for (std::uint32_t i = 0; i < count; ++i) {
        indices.push_back(0);
        indices.push_back(1 + i);
        indices.push_back(1 + (i + 1) % count);
    }
    canvas.mesh(texture, vertices, indices);
}

} // namespace opensu::ui

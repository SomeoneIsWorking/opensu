// round_shape — filled and banded rounded rectangles with per-vertex colour.
//
// iiSU paints tiles on an Android Canvas with shaders, blur mask filters and
// clip paths. raylib has none of those, so shapes are tessellated here into
// triangles whose vertex colours carry the gradient: a sweep or radial gradient
// is sampled at every vertex, a blur is a band whose alpha falls to zero, and a
// clip to a rounded rectangle is a textured fan of that shape.
#pragma once

#include <functional>

#include "raylib.h"

#include "home_layout.hpp"

namespace iideck::ui {

/// A rectangle with one corner radius.
struct RoundRect {
    Rect rect;
    float radius{};

    /// Grown (positive) or shrunk (negative) on every side, radius following.
    [[nodiscard]] RoundRect grown(float by) const noexcept;
};

/// A vertex colour from its position and its place across a band (0 inner, 1 outer).
using VertexColour = std::function<Color(Vector2 point, float across)>;

/// How finely outlines are cut: segments per corner and per straight edge.
struct Tessellation {
    int cornerSegments{8};
    int edgeSegments{1};
};

void fillRoundRect(const RoundRect& shape, const VertexColour& colour, Tessellation cut = {});

/// The area between two rounded rectangles, `outer` enclosing `inner`.
void fillBand(const RoundRect& outer, const RoundRect& inner, const VertexColour& colour,
              Tessellation cut = {});

/// A stroke centred on `centre`, `width` wide, its edges fading over `blur`, kept inside `clip`.
void strokeSoft(const RoundRect& centre, float width, float blur, const RoundRect& clip,
                const VertexColour& colour, Tessellation cut = {});

/// A filled shape whose edge fades over `blur` either side, as a blur mask softens it.
void fillSoft(const RoundRect& shape, float blur, const VertexColour& colour,
              Tessellation cut = {});

/// `source` of `texture` stretched over `shape`'s rectangle and clipped to its corners.
void drawTextureRound(const RoundRect& shape, const Texture& texture, Rectangle source, Color tint);

/// A colour with its alpha scaled.
[[nodiscard]] Color withAlpha(Color colour, float alpha) noexcept;
/// Straight interpolation between two colours.
[[nodiscard]] Color mix(Color from, Color to, float t) noexcept;

} // namespace iideck::ui

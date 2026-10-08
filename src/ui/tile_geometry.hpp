// tile_geometry — the rectangles and radii one grid tile is drawn with.
//
// iiSU tj2.V (GridTileGeometry) and the border pack's sprite proportions.
#pragma once

#include "home_layout.hpp"

namespace iideck::ui {

/// iiSU ux2 GridTileGeometry.
struct TileGeometry {
    Rect outer;
    Rect content;
    float frameWidth{};
    float outerRadius{};
    float contentRadius{};
    float outerStroke{};
    float innerShadowWidth{};
};

/// The geometry of a tile at `rect` in a grid whose cell's short side is `cell`.
[[nodiscard]] TileGeometry tileGeometry(const Rect& rect, float cell) noexcept;

/// A platform frame's proportions on its 1024 px sprite (iiSU assets/borders/*.png).
struct FrameGeometry {
    float stroke{};
    float outerRadius{};
    float tab{};
    float tabRadius{};
    /// iiSU g24.e: art is clipped at max(roundRadiusPct, 9)% of the short side.
    float artRadius{};
    /// iiSU g24.e: the square a console glyph is contain-fitted into, centred in the tab.
    Rect glyph;
};

/// The largest rectangle of `width` x `height`'s aspect that fits `slot`, centred in it.
[[nodiscard]] Rect containFit(float width, float height, const Rect& slot) noexcept;

/// The platform frame for a composition rectangle.
[[nodiscard]] FrameGeometry frameGeometry(const Rect& rect) noexcept;

} // namespace iideck::ui

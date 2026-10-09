// tile_geometry — the rectangles and radii one grid tile is drawn with.
//
// iiSU tj2.V (GridTileGeometry) and the border pack's sprite proportions.
#pragma once

#include <array>
#include <cstddef>

#include "home_layout.hpp"

namespace opensu::ui {

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

/// The rectangle to give `tileGeometry` so that its content, where the platform frame's edge shows,
/// is `content`. A rail lays tiles out by the slot their frame fills, not by the chrome around it.
[[nodiscard]] Rect outerForContent(const Rect& content) noexcept;

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

/// The most store icons one tile shows: Steam, GOG and Epic.
inline constexpr std::size_t maxStoreIcons = 3;

/// Where a game tile's store icons sit: round badges along the content's bottom-right corner,
/// the first leftmost, the last against the corner.
struct StoreIconRow {
    std::array<Rect, maxStoreIcons> badges{};
    std::size_t count{0};
};

/// The row for `count` icons (at most `maxStoreIcons`) on a tile whose content is `content`.
[[nodiscard]] StoreIconRow storeIconRow(const Rect& content, std::size_t count) noexcept;

/// The platform frame for a composition rectangle.
[[nodiscard]] FrameGeometry frameGeometry(const Rect& rect) noexcept;

} // namespace opensu::ui

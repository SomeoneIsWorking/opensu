// rail_layout — Library's XMB and Carousel layouts: one row or column of tiles in which the
// focused tile is larger, laid out for a focus position that moves continuously (iiSU `e39` for the
// vertical path, `lz3` for the horizontal one; navigation.md §2.2). Pure arithmetic: nothing here
// draws.
#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

/// What a rail is laid out from.
struct RailInput {
    float width{};
    float height{};
    /// One art aspect (width over height) per tile; 1 for a tile with no art.
    std::vector<float> aspects;
    /// The focus position in tiles, fractional while it moves; clamped to the tiles.
    float focus{};
    /// iiSU's icon size level, 1 to 20 (`xmbIconSizeLevel`, default 9).
    int iconLevel{9};
    /// Pixels per dp, which sizes what iiSU gives in dp: the titles and the left column.
    float dp{1.0f};
};

/// iiSU `w70.I`: the size level's scale, `clamp((level - 10) 0.11 + 1.45, 0.67, 2.55)`.
[[nodiscard]] float iconScale(int level) noexcept;

/// The tile under the point, given every tile's drawn rectangle: the tiles nearest `focus` are
/// drawn last, so they are on top.
[[nodiscard]] std::optional<std::size_t> railTileAt(const std::vector<Rect>& tiles, float focus,
                                                    float x, float y);

/// The vertical XMB: a column at the left edge, centred vertically (iiSU `e39`).
class XmbLayout {
  public:
    explicit XmbLayout(const RailInput& input);

    /// Distance of the column's left edge from the canvas's.
    [[nodiscard]] float leftMargin() const noexcept {
        return leftMargin_;
    }
    [[nodiscard]] float focusedSize() const noexcept {
        return focusedSize_;
    }
    [[nodiscard]] float unfocusedSize() const noexcept {
        return unfocusedSize_;
    }
    [[nodiscard]] float gap() const noexcept {
        return gap_;
    }
    [[nodiscard]] float centreY() const noexcept {
        return centreY_;
    }
    /// A tile's side before its aspect: `smoothstep` between the two sizes by its distance from
    /// the focus.
    [[nodiscard]] float sizeOf(std::size_t index) const noexcept;
    /// Every tile's rectangle, in tile order.
    [[nodiscard]] const std::vector<Rect>& rects() const noexcept {
        return rects_;
    }

    /// Pixels per dp the layout was made for.
    [[nodiscard]] float dp() const noexcept {
        return dp_;
    }
    /// The left column's section icon, centred on the centre line (navigation.md §5.3).
    [[nodiscard]] const Rect& sectionIcon() const noexcept {
        return sectionIcon_;
    }
    /// Where the console card stands in place of the icon inside a console (§5.4).
    [[nodiscard]] const Rect& headerCard() const noexcept {
        return headerCard_;
    }
    /// The centre of the marker that points from the column at the focused tile.
    [[nodiscard]] float markerX() const noexcept {
        return markerX_;
    }
    /// Where the focused tile's title starts, and the y its capitals' tops stand at.
    [[nodiscard]] float titleLeft() const noexcept {
        return titleLeft_;
    }
    [[nodiscard]] float titleCapTop() const noexcept {
        return titleCapTop_;
    }
    /// The title's capital height in pixels.
    [[nodiscard]] float titleCapHeight() const noexcept {
        return titleCapHeight_;
    }

  private:
    float focus_{};
    float dp_{1.0f};
    float leftMargin_{};
    float markerX_{};
    float titleLeft_{};
    float titleCapTop_{};
    float titleCapHeight_{};
    Rect sectionIcon_;
    Rect headerCard_;
    float focusedSize_{};
    float unfocusedSize_{};
    float gap_{};
    float centreY_{};
    std::vector<Rect> rects_;
};

/// The Carousel (horizontal XMB): a row centred on the focused tile, standing on a baseline low on
/// the canvas (iiSU `lz3`).
class CarouselLayout {
  public:
    explicit CarouselLayout(const RailInput& input);

    [[nodiscard]] float centreX() const noexcept {
        return centreX_;
    }
    /// The y the tiles' bottoms stand on.
    [[nodiscard]] float baselineY() const noexcept {
        return baselineY_;
    }
    [[nodiscard]] float focusedSize() const noexcept {
        return focusedSize_;
    }
    [[nodiscard]] float unfocusedSize() const noexcept {
        return unfocusedSize_;
    }
    [[nodiscard]] float gap() const noexcept {
        return gap_;
    }
    [[nodiscard]] float sizeOf(std::size_t index) const noexcept;
    [[nodiscard]] const std::vector<Rect>& rects() const noexcept {
        return rects_;
    }

    [[nodiscard]] float dp() const noexcept {
        return dp_;
    }
    /// The focused tile's title is centred on `centreX` with its capitals' tops at this y, and the
    /// marker under a focused game stands at `markerY` (navigation.md §5.3, §5.4).
    [[nodiscard]] float titleCapTop() const noexcept {
        return titleCapTop_;
    }
    [[nodiscard]] float titleCapHeight() const noexcept {
        return titleCapHeight_;
    }
    [[nodiscard]] float markerY() const noexcept {
        return markerY_;
    }

    /// The widest and narrowest a tile's slot is, as a multiple of its height (iiSU `w70.U`).
    static constexpr float narrowestSlot = 0.55f;
    static constexpr float widestSlot = 2.5f;

  private:
    float focus_{};
    float dp_{1.0f};
    float centreX_{};
    float baselineY_{};
    float titleCapTop_{};
    float titleCapHeight_{};
    float markerY_{};
    float focusedSize_{};
    float unfocusedSize_{};
    float gap_{};
    std::vector<Rect> rects_;
};

} // namespace opensu::ui

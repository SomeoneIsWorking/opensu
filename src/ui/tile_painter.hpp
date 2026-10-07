// tile_painter — draws one home grid tile the way iiSU's grid renderer does.
//
// Draw order is iiSU nx2.j: shadow, outer focus ring, glass chrome, art, inner
// focus ring, all scaled about the tile centre. The tile has no title and no
// badges: iiSU's grid path draws neither (home-grid.md §3.6, §3.8).
#pragma once

#include <string_view>

#include "raylib.h"

#include "home_layout.hpp"
#include "platform.hpp"
#include "round_shape.hpp"
#include "tile_geometry.hpp"

namespace iideck::ui {

/// Everything one tile is drawn from.
struct TileVisual {
    /// The tile's cell on the canvas, before scaling.
    Rect rect;
    /// The short side of one grid cell.
    float cell{};
    /// Focus scale, press pulse and entrance scale combined.
    float scale{1.0f};
    float alpha{1.0f};
    bool focused{false};
    /// An empty slot: chrome only.
    bool placeholder{false};
    /// Dark or light chrome (iiSU ya0.e).
    bool dark{false};
    float ringDegrees{0.0f};
    /// Cover art, or null for none.
    const Texture* art{nullptr};
    /// The platform frame, or null for an unframed tile.
    const Platform* platform{nullptr};
    std::string_view title;
};

struct ChromeVariant;

class TilePainter {
  public:
    void paint(const TileVisual& tile) const;

  private:
    void paintShadow(const TileGeometry& geometry, const ChromeVariant& variant, float alpha) const;
    void paintRing(const TileGeometry& geometry, float inset, float degrees, float alpha) const;
    void paintChrome(const TileGeometry& geometry, const ChromeVariant& variant, bool focused,
                     float alpha) const;
    void paintContent(const TileVisual& tile, const TileGeometry& geometry) const;
    void paintFrame(const Rect& rect, const Platform& platform, float alpha) const;
    void paintFallback(const Rect& content, std::string_view title, float alpha) const;
};

/// A cover crop of a `width` x `height` image for a `target` (iiSU e01.k, centred anchors).
[[nodiscard]] Rectangle coverSource(float width, float height, const Rect& target) noexcept;

} // namespace iideck::ui

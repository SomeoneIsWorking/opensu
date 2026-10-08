// tile_painter — draws one home grid tile the way iiSU's grid renderer does.
//
// Draw order is iiSU nx2.j: shadow, outer focus ring, glass chrome, art, inner
// focus ring, all scaled about the tile centre. A game tile has no title and no
// badges: iiSU's grid path draws neither (home-grid.md §3.6, §3.8). A console tile is iiSU's
// card for it when there is one, drawn as the whole tile; without, iideck's own: its platform's
// colours with the console's name and game count. A launcher and the combined library are
// iideck's own cards in the same shape, with the store's logo or a library glyph above the name.
// A store game that is owned in more than one store carries a row of store icons in its
// bottom-right corner.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "raylib.h"

#include "home_layout.hpp"
#include "platform.hpp"
#include "round_shape.hpp"
#include "tile_geometry.hpp"
#include "vector_icon.hpp"

namespace iideck::ui {

/// What a tile stands for.
enum class TileKind : std::uint8_t {
    Game,
    /// A console standing for its ROMs.
    Console,
    /// A store standing for its library.
    Launcher,
    /// The combined library.
    AllGames,
};

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
    /// Cover art, or a console's card, or null for none.
    const Texture* art{nullptr};
    /// The platform frame, or null for an unframed tile.
    const Platform* platform{nullptr};
    /// The console's glyph for the frame's tab, or null for none.
    const Texture* glyph{nullptr};
    std::string_view title;
    TileKind kind{TileKind::Game};
    /// A folder's second line, its game count.
    std::string_view caption;
    /// A launcher's logo.
    std::optional<Icon> logo;
    /// The stores a game is owned in, as icons in its corner.
    std::span<const Icon> stores;
};

struct ChromeVariant;

class TilePainter {
  public:
    void paint(const TileVisual& tile);

  private:
    void paintShadow(const TileGeometry& geometry, const ChromeVariant& variant, float alpha) const;
    void paintRing(const TileGeometry& geometry, float inset, float degrees, float alpha) const;
    void paintChrome(const TileGeometry& geometry, const ChromeVariant& variant, bool focused,
                     float alpha) const;
    void paintContent(const TileVisual& tile, const TileGeometry& geometry);
    void paintFrame(const Rect& rect, const Platform& platform, const Texture* glyph,
                    float alpha) const;
    void paintFallback(const Rect& content, std::string_view title, float alpha) const;
    /// A folder's card: a gradient with the mark, the name and the count centred on it.
    void paintCard(const TileVisual& tile, const Rect& content, Color from, Color to);
    void paintMark(const TileVisual& tile, const Rect& box, Color ink);
    void paintStores(const TileVisual& tile, const Rect& content);

    IconAtlas icons_;
};

/// A cover crop of a `width` x `height` image for a `target` (iiSU e01.k, centred anchors).
[[nodiscard]] Rectangle coverSource(float width, float height, const Rect& target) noexcept;

} // namespace iideck::ui

// tile_painter — draws one home grid tile the way iiSU's grid renderer does.
//
// Draw order is iiSU nx2.j: shadow, outer focus ring, glass chrome, art, inner
// focus ring, all scaled about the tile centre. A game tile has no title and no
// badges: iiSU's grid path draws neither (home-grid.md §3.6, §3.8). A console tile is openSU's
// card: its platform's colours with openSU's drawing of the console above its name and game
// count, in the platform frame with the console's glyph in the tab. A launcher and the combined
// library are cards in the same shape, with the store's logo or a library glyph above the name.
// A store game that is owned in more than one store carries a row of store icons in its
// bottom-right corner, and one its store has an update for a down arrow on the same disc in its
// top-right corner (opensu's own).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "paint.hpp"

#include "gpu2d/ui/typesetter.hpp"
#include "home_layout.hpp"
#include "platform.hpp"
#include "round_shape.hpp"
#include "tile_geometry.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// What a tile stands for.
enum class TileKind : std::uint8_t {
    Game,
    /// A console standing for its ROMs.
    Console,
    /// A store standing for its library.
    Launcher,
    /// The combined library.
    AllGames,
    /// The stores' free-to-keep offers.
    Giveaways,
    /// Adds a game of the player's own.
    AddGame,
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
    /// Whether the focused tile carries iiSU's cyan to violet selection ring: the grid's does, an
    /// XMB's and a Carousel's do not (navigation.md §5.3).
    bool selectionRing{true};
    /// An empty slot: chrome only.
    bool placeholder{false};
    /// Dark or light chrome (iiSU ya0.e).
    bool dark{false};
    float ringDegrees{0.0f};
    /// Cover art, or null for none.
    const gpu2d::Texture* art{nullptr};
    /// The platform frame, or null for an unframed tile.
    const Platform* platform{nullptr};
    /// The ES-DE system whose drawing a console tile carries and whose glyph a framed tile's tab
    /// carries; empty for none.
    std::string_view console;
    std::string_view title;
    TileKind kind{TileKind::Game};
    /// A folder's second line, its game count.
    std::string_view caption;
    /// A launcher's logo.
    std::optional<Icon> logo;
    /// The stores a game is owned in, as icons in its corner.
    std::span<const Icon> stores;
    /// The game's store has an update for it.
    bool update{false};
    /// Whether the cover is on its way (downloading, queued or decoding): a spinner stands in for
    /// the no-art letter.
    bool loading{false};
    /// The clock the spinner turns by.
    double seconds{0.0};
};

struct ChromeVariant;

class TilePainter {
  public:
    TilePainter(IconAtlas& icons, gpu2d::ui::Typesetter& text) noexcept
        : icons_{icons}, text_{text} {
    }

    void paint(Canvas& canvas, const TileVisual& tile);

  private:
    void paintShadow(Canvas& canvas, const TileGeometry& geometry, const ChromeVariant& variant,
                     float alpha) const;
    struct Ring {
        float inset;
        float degrees;
        float alpha;
    };
    void paintRing(Canvas& canvas, const TileGeometry& geometry, const Ring& ring) const;
    void paintChrome(Canvas& canvas, const TileGeometry& geometry, const ChromeVariant& variant,
                     bool focused, float alpha) const;
    void paintContent(Canvas& canvas, const TileVisual& tile, const TileGeometry& geometry);
    void paintFrame(Canvas& canvas, const Rect& rect, const Platform& platform,
                    std::string_view console, float alpha);
    void paintFallback(Canvas& canvas, const TileVisual& tile, const Rect& content) const;
    void paintLoading(Canvas& canvas, const TileVisual& tile, const Rect& content) const;
    /// A folder's card: a gradient with the mark, the name and the count centred on it.
    void paintCard(Canvas& canvas, const TileVisual& tile, const Rect& content, Colour from,
                   Colour to);
    void paintMark(Canvas& canvas, const TileVisual& tile, const Rect& box, Colour ink);
    void paintStores(Canvas& canvas, const TileVisual& tile, const Rect& content);
    void paintUpdate(Canvas& canvas, const TileVisual& tile, const Rect& content) const;

    IconAtlas& icons_;
    gpu2d::ui::Typesetter& text_;
};

/// A cover crop of a `width` x `height` image for a `target` (iiSU e01.k, centred anchors).
[[nodiscard]] gpu2d::Rect coverSource(float width, float height, const Rect& target) noexcept;

} // namespace opensu::ui

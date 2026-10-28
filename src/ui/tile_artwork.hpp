// tile_artwork — which of the home grid's tiles hold their art: the tiles near the canvas ask the
// image loader for theirs, a tile that leaves before its art lands lets the request go, and the
// oldest outside the window give their textures back when many are held.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "gpu2d/image_loader.hpp"

#include "home_layout.hpp"
#include "tile.hpp"

namespace opensu::ui {

/// The longest side a cover texture keeps; covers arrive up to 1200 x 1600 and more, and a tile
/// is never drawn that large, so larger ones are scaled down while decoding.
inline constexpr int maxCoverSide = 768;

class TileArtwork {
  public:
    /// Tiles holding art at most; the oldest outside the window let theirs go beyond it.
    static constexpr std::size_t residentLimit = 120;

    explicit TileArtwork(gpu2d::ImageLoader& loader) noexcept : loader_{loader} {
    }

    /// A new shelf replaces the old: every tile's art goes.
    void beginShelf(std::vector<Tile>& tiles, std::optional<Tile>& header);
    /// Lets a tile's art go; it is asked for again when the tile is next near the canvas.
    static void unload(Tile& tile);

    /// Asks for the art of the window's tiles and the header, cancels what tiles that left the
    /// window are still waiting for, and lets the oldest go beyond the limit.
    void request(std::vector<Tile>& tiles, std::optional<Tile>& header, const SlotRange& window);
    /// The window's tiles and the header still awaiting art: downloading, or not yet landed.
    [[nodiscard]] static std::size_t pending(const std::vector<Tile>& tiles,
                                             const std::optional<Tile>& header,
                                             const SlotRange& window);

  private:
    /// Asks for a tile's files; true when it now holds requests.
    bool ask(Tile& tile);
    void trim(std::vector<Tile>& tiles, const SlotRange& window);

    gpu2d::ImageLoader& loader_;
    /// Tiles holding requests, the oldest first.
    std::vector<std::uint64_t> resident_;
};

} // namespace opensu::ui

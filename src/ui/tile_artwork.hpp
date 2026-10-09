// tile_artwork — the textures of the home grid's tiles: which tiles' art is decoded (on the
// decoder's thread), uploaded (on the frame's), and given back again when many are held.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "home_layout.hpp"
#include "image_decoder.hpp"
#include "tile.hpp"

namespace opensu::ui {

class TileArtwork {
  public:
    /// Stands for the header tile in a decode job's tile index.
    static constexpr std::size_t headerTile = std::numeric_limits<std::size_t>::max();
    /// Textures held at most; the oldest outside the window are released beyond it.
    static constexpr std::size_t residentLimit = 120;

    /// A new shelf replaces the old: its textures go and answers for the old one are dropped.
    void beginShelf(std::vector<Tile>& tiles, std::optional<Tile>& header);
    /// A ticket for a tile whose files have changed, so a decode of the old ones is dropped.
    [[nodiscard]] std::uint64_t ticket() noexcept {
        return ++lastTicket_;
    }
    /// Gives back a tile's textures; its art is requested again when it is next near the canvas.
    void unload(Tile& tile);

    /// Queues the art of the window's tiles that has not been, uploads up to `uploads` decoded
    /// images and releases textures beyond the limit. Needs the GL context.
    void load(std::vector<Tile>& tiles, std::optional<Tile>& header, const SlotRange& window,
              std::size_t uploads);
    /// Whether every queued decode has been taken.
    [[nodiscard]] bool idle() const {
        return decoder_.idle();
    }
    void waitDecoded() {
        decoder_.waitDecoded();
    }

  private:
    [[nodiscard]] Tile* tileOf(std::vector<Tile>& tiles, std::optional<Tile>& header,
                               std::size_t index) const;
    void request(Tile& tile, std::size_t index);
    void requestWindow(std::vector<Tile>& tiles, std::optional<Tile>& header,
                       const SlotRange& window);
    void upload(std::vector<Tile>& tiles, std::optional<Tile>& header, std::size_t limit);
    void trim(std::vector<Tile>& tiles, const SlotRange& window);

    ImageDecoder decoder_;
    /// Replaced with the shelf, so decodes for an earlier one are dropped.
    std::uint64_t shelfSerial_{};
    std::uint64_t lastTicket_{};
    /// Tiles holding textures, the oldest first.
    std::vector<std::size_t> resident_;
};

} // namespace opensu::ui

#include "tile_artwork.hpp"

#include <algorithm>
#include <variant>

namespace opensu::ui {

void TileArtwork::beginShelf(std::vector<Tile>& tiles, std::optional<Tile>& header) {
    resident_.clear();
    for (Tile& tile : tiles) {
        unload(tile);
    }
    if (header) {
        unload(*header);
    }
}

void TileArtwork::unload(Tile& tile) {
    tile.portrait = {};
    tile.wide = {};
    tile.artRequested = false;
}

bool TileArtwork::ask(Tile& tile) {
    if (tile.artRequested) {
        return false;
    }
    tile.artRequested = true;
    const auto* game = std::get_if<library::Game>(&tile.item);
    if (game == nullptr) {
        return false;
    }
    // Covers are point-sampled.
    if (!game->artwork.empty()) {
        tile.portrait = loader_.loadFile(game->artwork, gpu2d::Filter::Nearest, maxCoverSide);
    }
    if (!game->artworkWide.empty()) {
        tile.wide = loader_.loadFile(game->artworkWide, gpu2d::Filter::Nearest, maxCoverSide);
    }
    return !game->artwork.empty() || !game->artworkWide.empty();
}

void TileArtwork::request(std::vector<Tile>& tiles, std::optional<Tile>& header,
                          const SlotRange& window) {
    for (std::size_t index = window.first; index < window.last && index < tiles.size(); ++index) {
        if (ask(tiles[index])) {
            resident_.push_back(tiles[index].id);
        }
    }
    if (header) {
        static_cast<void>(ask(*header));
    }
    trim(tiles, window);
}

void TileArtwork::trim(std::vector<Tile>& tiles, const SlotRange& window) {
    const auto inWindow = [&tiles, &window](const std::vector<Tile>::iterator at) {
        return window.contains(static_cast<std::size_t>(at - tiles.begin()));
    };
    // A tile that left the window before its art landed lets the decode go.
    std::erase_if(resident_, [&](std::uint64_t id) {
        const auto at = std::ranges::find(tiles, id, &Tile::id);
        if (at == tiles.end() || !at->artRequested) {
            return true;
        }
        if (!inWindow(at) && at->artLoading()) {
            unload(*at);
            return true;
        }
        return false;
    });
    // Oldest first: a tile outside the window gives its textures back until the limit holds.
    for (auto it = resident_.begin(); it != resident_.end() && resident_.size() > residentLimit;) {
        const auto at = std::ranges::find(tiles, *it, &Tile::id);
        if (inWindow(at)) {
            ++it;
            continue;
        }
        unload(*at);
        it = resident_.erase(it);
    }
}

std::size_t TileArtwork::pending(const std::vector<Tile>& tiles, const std::optional<Tile>& header,
                                 const SlotRange& window) {
    std::size_t count = header && header->awaitingArt() ? 1 : 0;
    for (std::size_t index = window.first; index < window.last && index < tiles.size(); ++index) {
        count += tiles[index].awaitingArt() ? 1 : 0;
    }
    return count;
}

} // namespace opensu::ui

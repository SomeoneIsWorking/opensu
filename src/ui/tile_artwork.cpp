#include "tile_artwork.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

/// A texture from decoded pixels, or an empty one for none.
Texture textureOf(const Pixels& pixels) {
    return pixels.empty() ? Texture{} : LoadTextureFromImage(pixels.image());
}

} // namespace

Tile* TileArtwork::tileOf(std::vector<Tile>& tiles, std::optional<Tile>& header,
                          std::size_t index) const {
    if (index == headerTile) {
        return header ? &*header : nullptr;
    }
    return index < tiles.size() ? &tiles[index] : nullptr;
}

void TileArtwork::beginShelf(std::vector<Tile>& tiles, std::optional<Tile>& header) {
    resident_.clear();
    for (Tile& tile : tiles) {
        unload(tile);
    }
    if (header) {
        unload(*header);
    }
    ++shelfSerial_;
}

void TileArtwork::unload(Tile& tile) {
    if (tile.hasPortrait) {
        UnloadTexture(tile.portrait);
        tile.hasPortrait = false;
    }
    if (tile.hasWide) {
        UnloadTexture(tile.wide);
        tile.hasWide = false;
    }
    tile.artLoaded = false;
    tile.artRequested = false;
    tile.ticket = ++lastTicket_;
}

void TileArtwork::request(Tile& tile, std::size_t index) {
    if (tile.artLoaded || tile.artRequested) {
        return;
    }
    DecodeJob job{
        .shelf = shelfSerial_, .tile = index, .ticket = tile.ticket, .portrait = {}, .wide = {}};
    if (const auto* console = std::get_if<library::Console>(&tile.item)) {
        job.portrait = console->artwork;
    } else if (const auto* game = std::get_if<library::Game>(&tile.item)) {
        job.portrait = game->artwork;
        job.wide = game->artworkWide;
    }
    if (job.portrait.empty() && job.wide.empty()) {
        tile.artLoaded = true;
        return;
    }
    tile.artRequested = true;
    decoder_.request(std::move(job));
}

void TileArtwork::requestWindow(std::vector<Tile>& tiles, std::optional<Tile>& header,
                                const SlotRange& window) {
    // Art queued for tiles that have since left the window is not worth decoding.
    for (const DecodeJob& dropped : decoder_.prune([&](const DecodeJob& job) {
             return job.shelf == shelfSerial_ &&
                    (job.tile == headerTile || window.contains(job.tile));
         })) {
        if (Tile* tile = dropped.shelf == shelfSerial_ ? tileOf(tiles, header, dropped.tile)
                                                       : nullptr) {
            tile->artRequested = false;
        }
    }
    for (std::size_t index = window.first; index < window.last; ++index) {
        request(tiles[index], index);
    }
    if (header) {
        request(*header, headerTile);
    }
}

void TileArtwork::upload(std::vector<Tile>& tiles, std::optional<Tile>& header,
                         std::size_t limit) {
    for (Decoded& done : decoder_.take(limit)) {
        Tile* tile = done.job.shelf == shelfSerial_ ? tileOf(tiles, header, done.job.tile) : nullptr;
        if (tile == nullptr || tile->ticket != done.job.ticket || tile->artLoaded) {
            continue;
        }
        tile->portrait = textureOf(done.portrait);
        tile->wide = textureOf(done.wide);
        tile->hasPortrait = tile->portrait.id != 0;
        tile->hasWide = tile->wide.id != 0;
        tile->artLoaded = true;
        tile->artRequested = false;
        if (done.job.tile != headerTile && (tile->hasPortrait || tile->hasWide)) {
            resident_.push_back(done.job.tile);
        }
    }
}

void TileArtwork::trim(std::vector<Tile>& tiles, const SlotRange& window) {
    std::erase_if(resident_, [&tiles](std::size_t index) {
        return index >= tiles.size() || !(tiles[index].hasPortrait || tiles[index].hasWide);
    });
    // Oldest first: a tile outside the window gives its textures back until the limit holds.
    for (auto it = resident_.begin(); it != resident_.end() && resident_.size() > residentLimit;) {
        if (window.contains(*it)) {
            ++it;
            continue;
        }
        unload(tiles[*it]);
        it = resident_.erase(it);
    }
}

void TileArtwork::load(std::vector<Tile>& tiles, std::optional<Tile>& header,
                       const SlotRange& window, std::size_t uploads) {
    requestWindow(tiles, header, window);
    upload(tiles, header, uploads);
    trim(tiles, window);
}

} // namespace opensu::ui

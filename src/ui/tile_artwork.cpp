#include "tile_artwork.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

/// A texture from decoded pixels, or nothing for none.
std::optional<gpu2d::Texture> textureOf(const TileArtwork::MakeTexture& make,
                                        const gpu2d::image::Bitmap& pixels) {
    if (pixels.empty()) {
        return std::nullopt;
    }
    return make(pixels);
}

} // namespace

Tile* TileArtwork::tileOf(std::vector<Tile>& tiles, std::optional<Tile>& header,
                          std::uint64_t id) const {
    if (header && header->id == id) {
        return &*header;
    }
    const auto at = std::ranges::find(tiles, id, &Tile::id);
    return at == tiles.end() ? nullptr : &*at;
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
    tile.portrait.reset();
    tile.wide.reset();
    tile.artLoaded = false;
    tile.artRequested = false;
    tile.ticket = ++lastTicket_;
}

void TileArtwork::request(Tile& tile, std::uint64_t id) {
    if (tile.artLoaded || tile.artRequested) {
        return;
    }
    DecodeJob job{
        .shelf = shelfSerial_, .tile = id, .ticket = tile.ticket, .portrait = {}, .wide = {}};
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
             if (job.shelf != shelfSerial_) {
                 return false;
             }
             if (header && job.tile == header->id) {
                 return true;
             }
             const auto at = std::ranges::find(tiles, job.tile, &Tile::id);
             return at != tiles.end() &&
                    window.contains(static_cast<std::size_t>(at - tiles.begin()));
         })) {
        if (Tile* tile =
                dropped.shelf == shelfSerial_ ? tileOf(tiles, header, dropped.tile) : nullptr) {
            tile->artRequested = false;
        }
    }
    for (std::size_t index = window.first; index < window.last && index < tiles.size(); ++index) {
        request(tiles[index], tiles[index].id);
    }
    if (header) {
        request(*header, header->id);
    }
}

void TileArtwork::upload(const MakeTexture& make, std::vector<Tile>& tiles,
                         std::optional<Tile>& header, std::size_t limit) {
    for (Decoded& done : decoder_.take(limit)) {
        Tile* tile =
            done.job.shelf == shelfSerial_ ? tileOf(tiles, header, done.job.tile) : nullptr;
        if (tile == nullptr || tile->ticket != done.job.ticket || tile->artLoaded) {
            continue;
        }
        tile->portrait = textureOf(make, done.portrait);
        tile->wide = textureOf(make, done.wide);
        tile->artLoaded = true;
        tile->artRequested = false;
        if ((header == std::nullopt || done.job.tile != header->id) && tile->art() != nullptr) {
            resident_.push_back(done.job.tile);
        }
    }
}

void TileArtwork::trim(std::vector<Tile>& tiles, const SlotRange& window) {
    std::erase_if(resident_, [&tiles](std::uint64_t id) {
        const auto at = std::ranges::find(tiles, id, &Tile::id);
        return at == tiles.end() || at->art() == nullptr;
    });
    // Oldest first: a tile outside the window gives its textures back until the limit holds.
    for (auto it = resident_.begin(); it != resident_.end() && resident_.size() > residentLimit;) {
        const auto at = std::ranges::find(tiles, *it, &Tile::id);
        if (at == tiles.end()) {
            it = resident_.erase(it);
            continue;
        }
        if (window.contains(static_cast<std::size_t>(at - tiles.begin()))) {
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

void TileArtwork::load(const MakeTexture& make, std::vector<Tile>& tiles,
                       std::optional<Tile>& header, const SlotRange& window, std::size_t uploads) {
    requestWindow(tiles, header, window);
    upload(make, tiles, header, uploads);
    trim(tiles, window);
}

} // namespace opensu::ui

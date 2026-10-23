// Tile artwork: a tile awaits its art from the request until its decode is uploaded, so a cover
// decoded but not yet taken by a frame still counts as pending.
#include "tile_artwork.hpp"

#include <cstdio>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::SlotRange;
using opensu::ui::Tile;
using opensu::ui::TileArtwork;

constexpr std::size_t everyUpload = std::numeric_limits<std::size_t>::max();

gpu2d::Texture noUpload(const gpu2d::image::Bitmap&) {
    throw std::logic_error{"a missing cover was uploaded"};
}

std::vector<Tile> shelf(TileArtwork& artwork, std::size_t count) {
    std::vector<Tile> tiles;
    for (std::size_t index = 0; index < count; ++index) {
        Tile& tile = tiles.emplace_back();
        tile.id = index + 1;
        tile.ticket = artwork.ticket();
        // A default item is a game.
        if (auto* game = std::get_if<opensu::library::Game>(&tile.item)) {
            game->id = "steam:" + std::to_string(index);
            // A file that is not there decodes to no pixels, so uploading needs no GPU.
            game->artwork = "missing-cover.png";
        }
    }
    return tiles;
}

void pendingUntilUploaded() {
    TileArtwork artwork;
    std::vector<Tile> tiles = shelf(artwork, 4);
    std::optional<Tile> header;
    artwork.beginShelf(tiles, header);
    const SlotRange window{.first = 0, .last = 3};
    tiles[3].artDownloading = true;
    expect(TileArtwork::pending(tiles, header, window) == 0, "nothing awaits before a request");

    artwork.load(noUpload, tiles, header, window, 0);
    expect(TileArtwork::pending(tiles, header, window) == 3,
           "every requested tile in the window awaits its art; the one outside does not count");
    artwork.waitDecoded();
    expect(TileArtwork::pending(tiles, header, window) == 3,
           "a decoded cover still awaits until a frame uploads it");

    artwork.load(noUpload, tiles, header, window, 1);
    expect(TileArtwork::pending(tiles, header, window) == 2, "an upload settles its tile");
    artwork.load(noUpload, tiles, header, window, everyUpload);
    expect(TileArtwork::pending(tiles, header, window) == 0, "every upload settles the window");

    tiles[1].artDownloading = true;
    expect(TileArtwork::pending(tiles, header, window) == 1,
           "a tile without art whose download is under way still awaits");
}

void headerCounts() {
    TileArtwork artwork;
    std::vector<Tile> tiles;
    std::optional<Tile> header = Tile{};
    header->artDownloading = true;
    expect(TileArtwork::pending(tiles, header, SlotRange{}) == 1, "the header's art counts");
}

} // namespace

int main() {
    pendingUntilUploaded();
    headerCounts();
    std::printf("tile_artwork: all checks passed\n");
    return 0;
}

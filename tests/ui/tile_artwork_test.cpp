// Tile artwork: a tile awaits its art from the request until the loader lands or fails it, so a
// cover decoded but not yet uploaded by a frame still counts as pending; a tile that leaves the
// window before its art lands lets the request go.
#include "tile_artwork.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "gpu2d/image/png_encoder.hpp"
#include "gpu2d/image_loader.hpp"

#include "check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::test::expect;
using opensu::ui::SlotRange;
using opensu::ui::Tile;
using opensu::ui::TileArtwork;

fs::path writeCover(const fs::path& dir) {
    fs::create_directories(dir);
    const fs::path file = dir / "cover.png";
    const std::vector<std::uint8_t> rgba(std::size_t{4} * 4 * 4, 200);
    std::string error;
    const std::string png = gpu2d::image::encodePng(rgba, 4, 4, error);
    expect(!png.empty(), "the fixture encodes");
    std::ofstream{file, std::ios::binary} << png;
    return file;
}

std::vector<Tile> shelf(std::size_t count, const fs::path& cover) {
    std::vector<Tile> tiles;
    for (std::size_t index = 0; index < count; ++index) {
        Tile& tile = tiles.emplace_back();
        tile.id = index + 1;
        // A default item is a game.
        if (auto* game = std::get_if<opensu::library::Game>(&tile.item)) {
            game->id = "steam:" + std::to_string(index);
            game->artwork = cover;
        }
    }
    return tiles;
}

void pendingUntilLanded(const fs::path& cover) {
    gpu2d::ImageLoader loader;
    TileArtwork artwork{loader};
    std::vector<Tile> tiles = shelf(4, cover);
    std::optional<Tile> header;
    const SlotRange window{.first = 0, .last = 3};
    tiles[3].artDownloading = true;
    expect(TileArtwork::pending(tiles, header, window) == 0, "nothing awaits before a request");

    artwork.request(tiles, header, window);
    expect(TileArtwork::pending(tiles, header, window) == 3,
           "every requested tile in the window awaits its art; the one outside does not count");
    loader.waitAll();
    expect(TileArtwork::pending(tiles, header, window) == 3,
           "a decoded cover still awaits until a frame uploads it");
    expect(tiles[0].art() == nullptr, "no art shows before it lands");

    tiles[1].artDownloading = true;
    artwork.beginShelf(tiles, header);
    expect(!tiles[0].artRequested && tiles[0].portrait.state() == gpu2d::ImageState::Empty,
           "a new shelf lets every tile's art go");
    expect(TileArtwork::pending(tiles, header, window) == 1,
           "a tile without art whose download is under way still awaits");
}

void failedArtSettles(const fs::path& dir) {
    gpu2d::ImageLoader loader;
    TileArtwork artwork{loader};
    std::vector<Tile> tiles = shelf(2, dir / "missing.png");
    std::optional<Tile> header;
    const SlotRange window{.first = 0, .last = 2};
    artwork.request(tiles, header, window);
    loader.waitAll();
    expect(TileArtwork::pending(tiles, header, window) == 0, "a cover that fails awaits no more");
    expect(tiles[0].portrait.state() == gpu2d::ImageState::Failed &&
               !tiles[0].portrait.error().empty(),
           "the failure says why");
}

void leavingTheWindowCancels(const fs::path& cover) {
    gpu2d::ImageLoader loader;
    TileArtwork artwork{loader};
    std::vector<Tile> tiles = shelf(4, cover);
    std::optional<Tile> header;
    artwork.request(tiles, header, SlotRange{.first = 0, .last = 2});
    expect(tiles[0].artRequested && tiles[1].artRequested, "the window's tiles ask for art");
    artwork.request(tiles, header, SlotRange{.first = 2, .last = 4});
    expect(!tiles[0].artRequested && !tiles[1].artRequested &&
               tiles[0].portrait.state() == gpu2d::ImageState::Empty,
           "tiles that left before their art landed let it go");
    expect(tiles[2].artRequested && tiles[3].artRequested, "the new window's tiles ask");
    loader.waitAll();
}

void headerCounts() {
    std::vector<Tile> tiles;
    std::optional<Tile> header = Tile{};
    header->artDownloading = true;
    expect(TileArtwork::pending(tiles, header, SlotRange{}) == 1, "the header's art counts");
}

} // namespace

int main() {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "tile_artwork";
    const fs::path cover = writeCover(dir);
    pendingUntilLanded(cover);
    failedArtSettles(dir);
    leavingTheWindowCancels(cover);
    headerCounts();
    std::printf("tile_artwork: all checks passed\n");
    return 0;
}

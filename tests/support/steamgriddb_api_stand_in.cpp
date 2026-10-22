// A stand-in SteamGridDB API for running openSU against: the test fake on loopback, holding for
// each Steam app id named a square, a portrait and a wide cover drawn here, each game in its own
// colour. Prints `api <root>` once listening and serves until its stdin closes.
//
//   steamgriddb_api_stand_in <key> <app id>...
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "gpu2d/image/png_encoder.hpp"
#include "support/steamgriddb_server.hpp"

namespace {

struct Shape {
    const char* name;
    int width;
    int height;
};

constexpr Shape shapes[] = {{"square", 512, 512}, {"portrait", 600, 900}, {"wide", 920, 430}};

/// Diagonal bands of `hue` and white, so a cover reads as a picture and its shape shows.
std::string drawn(int width, int height, std::uint8_t hue) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width * height) * 4U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool band = ((x + y) / 48) % 2 == 0;
            const std::uint8_t r = band ? hue : 250;
            const std::uint8_t g = band ? static_cast<std::uint8_t>(255 - hue) : 250;
            const std::uint8_t b = band ? 160 : 250;
            rgba.insert(rgba.end(), {r, g, b, 255});
        }
    }
    std::string error;
    std::string png = gpu2d::image::encodePng(rgba, static_cast<unsigned>(width),
                                              static_cast<unsigned>(height), error);
    if (png.empty()) {
        std::fprintf(stderr, "cannot draw a cover: %s\n", error.c_str());
    }
    return png;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <key> <app id>...\n", argv[0]);
        return 2;
    }
    opensu::test::SteamGridDbServer api{argv[1]};
    for (int i = 2; i < argc; ++i) {
        const std::string appId = argv[i];
        const auto hue = static_cast<std::uint8_t>(40 + (i - 2) * 50 % 200);
        for (const Shape& shape : shapes) {
            api.putSteam(appId, {appId + "-" + shape.name + ".png",
                                 drawn(shape.width, shape.height, hue), shape.width, shape.height});
        }
    }
    std::printf("api %s\n", api.api().c_str());
    std::fflush(stdout);
    std::string line;
    while (std::getline(std::cin, line)) {
    }
    return 0;
}

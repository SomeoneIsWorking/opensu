// Command opensu-svg-png writes an SVG file as a square PNG, through the shell's own rasteriser:
// the packaging's app icons (the macOS iconset).
//
//   opensu-svg-png <file.svg> <pixels> <file.png>
//
// Exit status: 0 when the PNG is written, 1 for a bad command line, an SVG that cannot be read or
// a PNG that cannot be written.
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "gpu2d/image/png_encoder.hpp"
#include "lucent/log.h"
#include "svg/svg_raster.hpp"

namespace {

std::optional<int> pixelsOf(std::string_view text) {
    int pixels = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), pixels);
    if (error != std::errc{} || end != text.data() + text.size() || pixels <= 0) {
        return std::nullopt;
    }
    return pixels;
}

} // namespace

int main(int argc, char** argv) try {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const std::optional<int> pixels = args.size() == 3 ? pixelsOf(args[1]) : std::nullopt;
    if (!pixels) {
        lucent::error("svg-png", "usage: opensu-svg-png <file.svg> <pixels> <file.png>");
        return 1;
    }
    const std::filesystem::path svg{args[0]};
    const std::filesystem::path png{args[2]};
    const std::optional<std::vector<unsigned char>> rgba =
        opensu::svg::rasterise(svg, *pixels, *pixels);
    if (!rgba) {
        lucent::error("svg-png", "cannot read {}", svg.string());
        return 1;
    }
    std::string error;
    const auto side = static_cast<unsigned>(*pixels);
    const std::string bytes =
        gpu2d::image::encodePng(std::span<const std::uint8_t>{*rgba}, side, side, error);
    if (bytes.empty()) {
        lucent::error("svg-png", "cannot encode {}: {}", png.string(), error);
        return 1;
    }
    std::ofstream out{png, std::ios::binary};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
        lucent::error("svg-png", "cannot write {}", png.string());
        return 1;
    }
    return 0;
} catch (const std::exception& failure) {
    lucent::error("svg-png", "stopped by an error: {}", failure.what());
    return 1;
}

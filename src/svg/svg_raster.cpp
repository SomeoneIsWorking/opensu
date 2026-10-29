#include "svg_raster.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <nanosvg.h>
#include <nanosvgrast.h>

namespace opensu::svg {
namespace {

struct ImageDeleter {
    void operator()(NSVGimage* image) const noexcept {
        nsvgDelete(image);
    }
};
struct RasterizerDeleter {
    void operator()(NSVGrasterizer* rasterizer) const noexcept {
        nsvgDeleteRasterizer(rasterizer);
    }
};

constexpr std::size_t channels = 4;

/// nanosvg's packed colour, red in the low byte, as RGB without its alpha.
constexpr unsigned int rgbMask = 0x00FFFFFFu;

unsigned int swapped(unsigned int colour, std::span<const ColourSwap> swaps) noexcept {
    for (const ColourSwap& swap : swaps) {
        if ((colour & rgbMask) == packedRgb(swap.from)) {
            return (colour & ~rgbMask) | packedRgb(swap.to);
        }
    }
    return colour;
}

void swapPaint(NSVGpaint& paint, std::span<const ColourSwap> swaps) noexcept {
    if (paint.type == NSVG_PAINT_COLOR) {
        paint.color = swapped(paint.color, swaps);
        return;
    }
    if (paint.type == NSVG_PAINT_LINEAR_GRADIENT || paint.type == NSVG_PAINT_RADIAL_GRADIENT) {
        NSVGgradient& gradient = *paint.gradient;
        // nanosvg allocates the stops past the end of the struct.
        const std::span<NSVGgradientStop> stops{gradient.stops,
                                                static_cast<std::size_t>(gradient.nstops)};
        for (NSVGgradientStop& stop : stops) {
            stop.color = swapped(stop.color, swaps);
        }
    }
}

} // namespace

unsigned int packedRgb(const gpu2d::Colour& colour) noexcept {
    return static_cast<unsigned int>(colour.r) | (static_cast<unsigned int>(colour.g) << 8u) |
           (static_cast<unsigned int>(colour.b) << 16u);
}

std::optional<std::vector<unsigned char>> rasterise(const std::filesystem::path& svg, int width,
                                                    int height, std::span<const ColourSwap> swaps) {
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    std::ifstream file{svg, std::ios::binary};
    std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if (text.empty()) {
        return std::nullopt;
    }
    // nsvgParse edits the text in place.
    const std::unique_ptr<NSVGimage, ImageDeleter> image{nsvgParse(text.data(), "px", 96.0f)};
    if (image == nullptr || image->width <= 0.0f || image->height <= 0.0f) {
        return std::nullopt;
    }
    const std::unique_ptr<NSVGrasterizer, RasterizerDeleter> rasterizer{nsvgCreateRasterizer()};
    if (rasterizer == nullptr) {
        return std::nullopt;
    }
    if (!swaps.empty()) {
        for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
            swapPaint(shape->fill, swaps);
            swapPaint(shape->stroke, swaps);
        }
    }
    const auto across = static_cast<float>(width);
    const auto down = static_cast<float>(height);
    const float scale = std::min(across / image->width, down / image->height);
    const float dx = (across - image->width * scale) * 0.5f;
    const float dy = (down - image->height * scale) * 0.5f;
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<unsigned char> rgba(count * channels, 0);
    nsvgRasterize(rasterizer.get(), image.get(), dx, dy, scale, rgba.data(), width, height,
                  width * static_cast<int>(channels));
    return rgba;
}

std::optional<std::vector<unsigned char>> rasteriseMask(const std::filesystem::path& svg,
                                                        int pixels) {
    std::optional<std::vector<unsigned char>> rgba = rasterise(svg, pixels, pixels);
    if (!rgba) {
        return std::nullopt;
    }
    // The logos are single-colour marks; the tint picks the colour they are drawn in.
    for (std::size_t i = 0; i < rgba->size(); i += channels) {
        (*rgba)[i] = 255;
        (*rgba)[i + 1] = 255;
        (*rgba)[i + 2] = 255;
    }
    return rgba;
}

} // namespace opensu::svg

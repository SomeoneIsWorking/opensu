#include "vector_icon.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

namespace opensu::ui {
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

unsigned int packedRgb(const gpu2d::Colour& colour) noexcept {
    return static_cast<unsigned int>(colour.r) | (static_cast<unsigned int>(colour.g) << 8u) |
           (static_cast<unsigned int>(colour.b) << 16u);
}

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

bool sameColour(const gpu2d::Colour& a, const gpu2d::Colour& b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool sameSwaps(std::span<const ColourSwap> a, std::span<const ColourSwap> b) noexcept {
    return std::ranges::equal(a, b, [](const ColourSwap& x, const ColourSwap& y) {
        return sameColour(x.from, y.from) && sameColour(x.to, y.to);
    });
}

} // namespace

std::optional<Icon> iconFor(library::Source source) noexcept {
    switch (source) {
    case library::Source::Steam:
        return Icon::Steam;
    case library::Source::Epic:
        return Icon::Epic;
    case library::Source::Gog:
        return Icon::Gog;
    case library::Source::Rom:
    case library::Source::Own:
        break;
    }
    return std::nullopt;
}

std::string_view iconFile(Icon icon) noexcept {
    switch (icon) {
    case Icon::Steam:
        return "icons/steam.svg";
    case Icon::Epic:
        return "icons/epicgames.svg";
    case Icon::Gog:
        return "icons/gogdotcom.svg";
    case Icon::Home:
        return "icons/home.svg";
    case Icon::HomeSelected:
        return "icons/home_selected.svg";
    case Icon::Library:
        return "icons/library.svg";
    case Icon::LibrarySelected:
        return "icons/library_selected.svg";
    }
    return {};
}

std::string consoleCardFile(std::string_view system) {
    return "consoles/" + std::string{system} + ".svg";
}

std::string consoleGlyphFile(std::string_view system) {
    return "consoles/glyphs/" + std::string{system} + ".svg";
}

Icon dockIcon(library::Section section, bool selected) noexcept {
    if (section == library::Section::Home) {
        return selected ? Icon::HomeSelected : Icon::Home;
    }
    return selected ? Icon::LibrarySelected : Icon::Library;
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

const gpu2d::Texture* IconAtlas::mask(Icon icon, int pixels) {
    return find(iconFile(icon), pixels, pixels, true, {});
}

const gpu2d::Texture* IconAtlas::mask(std::string_view file, int pixels) {
    return find(file, pixels, pixels, true, {});
}

const gpu2d::Texture* IconAtlas::image(Icon icon, int width, int height,
                                       std::span<const ColourSwap> swaps) {
    return find(iconFile(icon), width, height, false, swaps);
}

const gpu2d::Texture* IconAtlas::find(std::string_view file, int width, int height, bool mask,
                                      std::span<const ColourSwap> swaps) {
    const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) {
        return entry.file == file && entry.width == width && entry.height == height &&
               entry.mask == mask && sameSwaps(entry.swaps, swaps);
    });
    if (found != entries_.end()) {
        return found->image.texture();
    }
    std::vector<ColourSwap> colours{swaps.begin(), swaps.end()};
    std::string key = (assetsDir_ / file).string() + " " + std::to_string(width) + "x" +
                      std::to_string(height) + (mask ? " mask" : "");
    for (const ColourSwap& swap : colours) {
        key +=
            " " + std::to_string(packedRgb(swap.from)) + ">" + std::to_string(packedRgb(swap.to));
    }
    gpu2d::ImageHandle image = images_.load(gpu2d::ImageRequest{
        .key = std::move(key),
        .decode = [path = assetsDir_ / file, width, height, mask,
                   colours](std::string& error) -> std::optional<gpu2d::image::Bitmap> {
            std::optional<std::vector<unsigned char>> rgba =
                mask ? rasteriseMask(path, width) : rasterise(path, width, height, colours);
            if (!rgba) {
                error = "the SVG cannot be read";
                return std::nullopt;
            }
            return gpu2d::image::Bitmap{.width = width, .height = height, .rgba = std::move(*rgba)};
        },
        .filter = gpu2d::Filter::Linear});
    const Entry& entry = entries_.emplace_back(Entry{.file = std::string{file},
                                                     .width = width,
                                                     .height = height,
                                                     .mask = mask,
                                                     .swaps = std::move(colours),
                                                     .image = std::move(image)});
    return entry.image.texture();
}

bool IconAtlas::pending() const noexcept {
    return std::ranges::any_of(entries_, [](const Entry& entry) {
        return entry.image.pending();
    });
}

void IconAtlas::wait() {
    for (const Entry& entry : entries_) {
        if (entry.image.pending()) {
            images_.wait(entry.image);
        }
    }
}

} // namespace opensu::ui

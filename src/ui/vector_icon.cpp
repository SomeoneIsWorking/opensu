#include "vector_icon.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace opensu::ui {
namespace {

using svg::ColourSwap;

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

Icon dockIcon(library::Section section, bool selected) noexcept {
    if (section == library::Section::Home) {
        return selected ? Icon::HomeSelected : Icon::Home;
    }
    return selected ? Icon::LibrarySelected : Icon::Library;
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
        key += " " + std::to_string(svg::packedRgb(swap.from)) + ">" +
               std::to_string(svg::packedRgb(swap.to));
    }
    gpu2d::ImageHandle image = images_.load(gpu2d::ImageRequest{
        .key = std::move(key),
        .decode = [path = assetsDir_ / file, width, height, mask,
                   colours](std::string& error) -> std::optional<gpu2d::image::Bitmap> {
            std::optional<std::vector<unsigned char>> rgba =
                mask ? svg::rasteriseMask(path, width)
                     : svg::rasterise(path, width, height, colours);
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

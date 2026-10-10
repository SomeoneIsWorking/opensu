#include "vector_icon.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

#include "lucent/log.h"

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
    }
    return {};
}

std::optional<std::vector<unsigned char>> rasteriseMask(const std::filesystem::path& svg,
                                                        int pixels) {
    if (pixels <= 0) {
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
    const auto side = static_cast<float>(pixels);
    const float scale = side / std::max(image->width, image->height);
    const float dx = (side - image->width * scale) * 0.5f;
    const float dy = (side - image->height * scale) * 0.5f;
    const auto count = static_cast<std::size_t>(pixels) * static_cast<std::size_t>(pixels);
    std::vector<unsigned char> rgba(count * channels, 0);
    nsvgRasterize(rasterizer.get(), image.get(), dx, dy, scale, rgba.data(), pixels, pixels,
                  pixels * static_cast<int>(channels));
    // The logos are single-colour marks; the tint picks the colour they are drawn in.
    for (std::size_t i = 0; i < count; ++i) {
        rgba[i * channels] = 255;
        rgba[i * channels + 1] = 255;
        rgba[i * channels + 2] = 255;
    }
    return rgba;
}

IconAtlas::~IconAtlas() {
    for (const Entry& entry : entries_) {
        if (entry.loaded) {
            UnloadTexture(entry.texture);
        }
    }
}

const Texture* IconAtlas::mask(Icon icon, int pixels) {
    const auto found = std::ranges::find_if(entries_, [icon, pixels](const Entry& entry) {
        return entry.icon == icon && entry.pixels == pixels;
    });
    if (found != entries_.end()) {
        return found->loaded ? &found->texture : nullptr;
    }
    Entry entry{.icon = icon, .pixels = pixels};
    const std::filesystem::path path = assetsDir_ / iconFile(icon);
    if (std::optional<std::vector<unsigned char>> rgba = rasteriseMask(path, pixels)) {
        Image image{.data = rgba->data(),
                    .width = pixels,
                    .height = pixels,
                    .mipmaps = 1,
                    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        entry.texture = LoadTextureFromImage(image);
        entry.loaded = entry.texture.id != 0;
        SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
    } else {
        lucent::error("ui", "icon {} could not be read", path.string());
    }
    entries_.push_back(entry);
    return entries_.back().loaded ? &entries_.back().texture : nullptr;
}

} // namespace opensu::ui

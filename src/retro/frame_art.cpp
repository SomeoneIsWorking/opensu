#include "frame_art.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "frame_compositor.hpp"
#include "gpu2d/image/decode.hpp"
#include "lucent/log.h"

namespace opensu::retro {
namespace {

std::int32_t scaledBy(std::uint32_t length, float scale) {
    return static_cast<std::int32_t>(std::lround(static_cast<float>(length) * scale));
}

/// `art` decoded, or nothing, said in the log, when it cannot be shown.
std::optional<ArtImage> decoded(const settings::FrameArt& art) {
    std::string error;
    std::optional<gpu2d::image::Bitmap> bitmap = gpu2d::image::decodeFile(art.image, error);
    if (!bitmap || bitmap->empty()) {
        lucent::warn("retro", "the frame {} cannot be read: {}", art.image.string(), error);
        return std::nullopt;
    }
    const auto width = static_cast<std::uint32_t>(bitmap->width);
    const auto height = static_cast<std::uint32_t>(bitmap->height);
    if (art.x + art.width > width || art.y + art.height > height) {
        lucent::warn("retro", "the frame {} is {}x{}, its picture rect leaves it",
                     art.image.string(), width, height);
        return std::nullopt;
    }
    lucent::info("retro", "the frame {} ({}x{}) drawn around the picture", art.image.string(),
                 width, height);
    return ArtImage{.art = art, .width = width, .height = height, .rgba = std::move(bitmap->rgba)};
}

void keep(std::optional<ArtImage>& held, const std::optional<settings::FrameArt>& wanted) {
    if (!wanted) {
        held.reset();
        return;
    }
    if (held && held->art == *wanted) {
        return;
    }
    held = decoded(*wanted);
}

} // namespace

Framing frameIn(VkRect2D room, const ArtImage& image) {
    VkRect2D art =
        letterbox(room.extent, static_cast<float>(image.width) / static_cast<float>(image.height));
    art.offset.x += room.offset.x;
    art.offset.y += room.offset.y;
    const float scale = static_cast<float>(art.extent.width) / static_cast<float>(image.width);
    VkRect2D picture{};
    picture.offset = {art.offset.x + scaledBy(image.art.x, scale),
                      art.offset.y + scaledBy(image.art.y, scale)};
    picture.extent = {static_cast<std::uint32_t>(std::max(1, scaledBy(image.art.width, scale))),
                      static_cast<std::uint32_t>(std::max(1, scaledBy(image.art.height, scale)))};
    return Framing{.art = art, .picture = picture};
}

void FrameArtImages::load(const settings::VideoSettings& video) {
    keep(landscape_, video.landscapeArt);
    keep(portrait_, video.portraitArt);
}

const ArtImage* FrameArtImages::forRoom(VkExtent2D extent) const {
    const std::optional<ArtImage>& art = extent.height > extent.width ? portrait_ : landscape_;
    return art ? &*art : nullptr;
}

} // namespace opensu::retro

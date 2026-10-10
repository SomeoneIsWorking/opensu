#include "rail_painter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "icon_recolour.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// navigation.md §5.3: the title and the markers are #4D4655 in the light theme, white in the dark.
constexpr Color lightInk{0x4D, 0x46, 0x55, 255};
constexpr Color darkInk{0xFF, 0xFF, 0xFF, 255};
// The marker's size in dp, measured on the ◂ (18 x 28 px at 2.25 px per dp).
constexpr float markerLongDp = 12.4f;
constexpr float markerShortDp = 8.0f;
// The title's soft shadow, offset down and right; its blur is a ring of faint copies.
// guess: the capture shows a soft shadow but its numbers are unrecovered.
constexpr float shadowOffsetDp = 1.0f;
constexpr float shadowRadiusDp = 2.0f;
constexpr int shadowTaps = 8;
constexpr float shadowAlpha = 0.045f;

Color inkOf(bool dark, float alpha) noexcept {
    return Fade(dark ? darkInk : lightInk, alpha);
}

Rgb rgbOf(std::uint32_t colour) noexcept {
    return Rgb{static_cast<std::uint8_t>((colour >> 16) & 0xFF),
               static_cast<std::uint8_t>((colour >> 8) & 0xFF),
               static_cast<std::uint8_t>(colour & 0xFF)};
}

} // namespace

RailPainter::~RailPainter() {
    unloadTints();
}

void RailPainter::unloadTints() {
    for (auto& [key, texture] : tints_) {
        UnloadTexture(texture);
    }
    tints_.clear();
}

void RailPainter::setSectionIcon(const std::filesystem::path& file) {
    if (file == sectionIcon_) {
        return;
    }
    unloadTints();
    sectionIcon_ = file;
}

namespace {

/// The colours a card's border runs between, read from its file; nothing when it will not load.
std::optional<Gradient> cardGradient(const std::filesystem::path& card) {
    if (card.empty()) {
        return std::nullopt;
    }
    Image image = LoadImage(card.string().c_str());
    if (image.data == nullptr) {
        return std::nullopt;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const auto bytes =
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4;
    const Gradient gradient = borderColours(
        std::span{static_cast<const std::uint8_t*>(image.data), bytes}, image.width, image.height);
    UnloadImage(image);
    return gradient;
}

} // namespace

const Texture* RailPainter::tinted(const Focused& focused) {
    if (sectionIcon_.empty()) {
        return nullptr;
    }
    std::string key = focused.key;
    if (key.empty() && focused.platform != nullptr) {
        key = focused.platform->key;
    }
    if (const auto found = tints_.find(key); found != tints_.end()) {
        return found->second.id != 0 ? &found->second : nullptr;
    }
    std::optional<Gradient> colours = cardGradient(focused.card);
    if (!colours && focused.platform != nullptr) {
        colours = Gradient{rgbOf(focused.platform->strokeFrom), rgbOf(focused.platform->strokeTo)};
    }
    Image image = LoadImage(sectionIcon_.string().c_str());
    Texture texture{};
    if (image.data != nullptr) {
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (colours) {
            const auto bytes =
                static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4;
            recolourIcon(std::span{static_cast<std::uint8_t*>(image.data), bytes}, image.width,
                         image.height, colours->from, colours->to);
        }
        texture = LoadTextureFromImage(image);
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    UnloadImage(image);
    const auto inserted = tints_.emplace(key, texture).first;
    return texture.id != 0 ? &inserted->second : nullptr;
}

void RailPainter::paintColumn(const XmbLayout& layout, const Focused& focused,
                              const RailStyle& style) {
    if (const Texture* icon = tinted(focused)) {
        const Rect& box = layout.sectionIcon();
        DrawTexturePro(*icon,
                       Rectangle{0.0f, 0.0f, static_cast<float>(icon->width),
                                 static_cast<float>(icon->height)},
                       Rectangle{box.x, box.y, box.width, box.height}, Vector2{0.0f, 0.0f}, 0.0f,
                       Fade(WHITE, style.alpha));
    }
    paintColumnMarker(layout, style);
}

void RailPainter::paintColumnMarker(const XmbLayout& layout, const RailStyle& style) const {
    // ◂ points left, at the column from the tile beside it.
    const float dp = layout.dp();
    const float x = layout.markerX();
    const float y = layout.sectionIcon().centreY();
    const float half = markerLongDp * dp * 0.5f;
    const float depth = markerShortDp * dp * 0.5f;
    DrawTriangle(Vector2{x - depth, y}, Vector2{x + depth, y + half}, Vector2{x + depth, y - half},
                 inkOf(style.dark, style.alpha));
}

void RailPainter::paintRowMarker(const CarouselLayout& layout, const RailStyle& style) const {
    // ▾ points down, at the focused game from under it.
    const float dp = layout.dp();
    const float half = markerLongDp * dp * 0.5f;
    const float depth = markerShortDp * dp * 0.5f;
    const float x = layout.centreX();
    const float y = layout.markerY();
    DrawTriangle(Vector2{x - half, y - depth}, Vector2{x, y + depth}, Vector2{x + half, y - depth},
                 inkOf(style.dark, style.alpha));
}

void RailPainter::drawTitle(std::string_view title, const TitleBox& box, const RailStyle& style) {
    if (title.empty() || style.alpha <= 0.0f) {
        return;
    }
    const TextStyle text{box.capHeight / Typeface::capHeightPerEm};
    const float left = box.centred ? box.x - typeface_.measure(title, text) * 0.5f : box.x;
    const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(shadowTaps);
    for (int tap = 0; tap < shadowTaps; ++tap) {
        const float angle = step * static_cast<float>(tap);
        typeface_.drawFromCapTop(
            title,
            Vector2{left + (shadowOffsetDp + shadowRadiusDp * std::cos(angle)) * box.dp,
                    box.capTop + (shadowOffsetDp + shadowRadiusDp * std::sin(angle)) * box.dp},
            text, Fade(BLACK, shadowAlpha * style.alpha));
    }
    typeface_.drawFromCapTop(title, Vector2{left, box.capTop}, text,
                             inkOf(style.dark, style.alpha));
}

void RailPainter::paintTitle(const XmbLayout& layout, std::string_view title,
                             const RailStyle& style) {
    drawTitle(title,
              TitleBox{layout.titleLeft(), layout.titleCapTop(), layout.titleCapHeight(),
                       layout.dp(), false},
              style);
}

void RailPainter::paintTitle(const CarouselLayout& layout, std::string_view title,
                             const RailStyle& style) {
    drawTitle(title,
              TitleBox{layout.centreX(), layout.titleCapTop(), layout.titleCapHeight(), layout.dp(),
                       true},
              style);
}

} // namespace opensu::ui

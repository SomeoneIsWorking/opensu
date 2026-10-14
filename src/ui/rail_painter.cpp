#include "rail_painter.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "gpu2d/image/decode.hpp"
#include "icon_recolour.hpp"
#include "lucent/log.h"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// navigation.md §5.3: the title and the markers are #4D4655 in the light theme, white in the dark.
constexpr Colour lightInk{0x4D, 0x46, 0x55, 255};
constexpr Colour darkInk{0xFF, 0xFF, 0xFF, 255};
// The marker's size in dp, measured on the ◂ (18 x 28 px at 2.25 px per dp).
constexpr float markerLongDp = 12.4f;
constexpr float markerShortDp = 8.0f;
// The title's soft shadow, offset down and right; its blur is a ring of faint copies.
// guess: the capture shows a soft shadow but its numbers are unrecovered.
constexpr float shadowOffsetDp = 1.0f;
constexpr float shadowRadiusDp = 2.0f;
constexpr int shadowTaps = 8;
constexpr float shadowAlpha = 0.045f;

Colour inkOf(bool dark, float alpha) noexcept {
    return withAlpha(dark ? darkInk : lightInk, alpha);
}

Rgb rgbOf(std::uint32_t colour) noexcept {
    return Rgb{static_cast<std::uint8_t>((colour >> 16) & 0xFF),
               static_cast<std::uint8_t>((colour >> 8) & 0xFF),
               static_cast<std::uint8_t>(colour & 0xFF)};
}

} // namespace

void RailPainter::setSectionIcon(const std::filesystem::path& file) {
    if (file == sectionIcon_) {
        return;
    }
    tints_.clear();
    sectionIcon_ = file;
}

namespace {

/// The colours a card's border runs between, read from its file; nothing when it will not load.
std::optional<Gradient> cardGradient(const std::filesystem::path& card) {
    if (card.empty()) {
        return std::nullopt;
    }
    std::string error;
    const std::optional<gpu2d::image::Bitmap> image = gpu2d::image::decodeFile(card, error);
    if (!image) {
        lucent::warn("ui", "{}: {}", card.string(), error);
        return std::nullopt;
    }
    return borderColours(image->rgba, image->width, image->height);
}

} // namespace

const gpu2d::Texture* RailPainter::tinted(const Focused& focused) {
    if (sectionIcon_.empty()) {
        return nullptr;
    }
    std::string key = focused.key;
    if (key.empty() && focused.platform != nullptr) {
        key = focused.platform->key;
    }
    if (const auto found = tints_.find(key); found != tints_.end()) {
        return found->second ? &*found->second : nullptr;
    }
    if (device_ == nullptr) {
        throw std::logic_error{"RailPainter needs a device before it paints the section icon"};
    }
    std::optional<Gradient> colours = cardGradient(focused.card);
    if (!colours && focused.platform != nullptr) {
        colours = Gradient{rgbOf(focused.platform->strokeFrom), rgbOf(focused.platform->strokeTo)};
    }
    std::string error;
    std::optional<gpu2d::image::Bitmap> image = gpu2d::image::decodeFile(sectionIcon_, error);
    std::optional<gpu2d::Texture> texture;
    if (image) {
        if (colours) {
            recolourIcon(image->rgba, image->width, image->height, colours->from, colours->to);
        }
        texture.emplace(*device_, image->rgba, image->width, image->height, gpu2d::Filter::Linear);
    } else {
        lucent::warn("ui", "{}: {}", sectionIcon_.string(), error);
    }
    const auto inserted = tints_.emplace(key, std::move(texture)).first;
    return inserted->second ? &*inserted->second : nullptr;
}

void RailPainter::paintColumn(Canvas& canvas, const XmbLayout& layout, const Focused& focused,
                              const RailStyle& style) {
    if (const gpu2d::Texture* icon = tinted(focused)) {
        const Rect& box = layout.sectionIcon();
        canvas.image(icon->view(), wholeOf(*icon), onCanvas(box), withAlpha(white, style.alpha));
    }
    paintColumnMarker(canvas, layout, style);
}

void RailPainter::paintColumnMarker(Canvas& canvas, const XmbLayout& layout,
                                    const RailStyle& style) const {
    // ◂ points left, at the column from the tile beside it.
    const float dp = layout.dp();
    const float x = layout.markerX();
    const float y = layout.sectionIcon().centreY();
    const float half = markerLongDp * dp * 0.5f;
    const float depth = markerShortDp * dp * 0.5f;
    canvas.triangle(Point{x - depth, y}, Point{x + depth, y + half}, Point{x + depth, y - half},
                    inkOf(style.dark, style.alpha));
}

void RailPainter::paintRowMarker(Canvas& canvas, const CarouselLayout& layout,
                                 const RailStyle& style) const {
    // ▾ points down, at the focused game from under it.
    const float dp = layout.dp();
    const float half = markerLongDp * dp * 0.5f;
    const float depth = markerShortDp * dp * 0.5f;
    const float x = layout.centreX();
    const float y = layout.markerY();
    canvas.triangle(Point{x - half, y - depth}, Point{x, y + depth}, Point{x + half, y - depth},
                    inkOf(style.dark, style.alpha));
}

void RailPainter::drawTitle(Canvas& canvas, std::string_view title, const TitleBox& box,
                            const RailStyle& style) {
    if (title.empty() || style.alpha <= 0.0f) {
        return;
    }
    const TextStyle text{box.capHeight / Typeface::capHeightPerEm};
    const float left = box.centred ? box.x - typeface_.measure(title, text) * 0.5f : box.x;
    const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(shadowTaps);
    for (int tap = 0; tap < shadowTaps; ++tap) {
        const float angle = step * static_cast<float>(tap);
        typeface_.drawFromCapTop(
            canvas, title,
            Point{left + (shadowOffsetDp + shadowRadiusDp * std::cos(angle)) * box.dp,
                  box.capTop + (shadowOffsetDp + shadowRadiusDp * std::sin(angle)) * box.dp},
            text, withAlpha(black, shadowAlpha * style.alpha));
    }
    typeface_.drawFromCapTop(canvas, title, Point{left, box.capTop}, text,
                             inkOf(style.dark, style.alpha));
}

void RailPainter::paintTitle(Canvas& canvas, const XmbLayout& layout, std::string_view title,
                             const RailStyle& style) {
    drawTitle(canvas, title,
              TitleBox{layout.titleLeft(), layout.titleCapTop(), layout.titleCapHeight(),
                       layout.dp(), false},
              style);
}

void RailPainter::paintTitle(Canvas& canvas, const CarouselLayout& layout, std::string_view title,
                             const RailStyle& style) {
    drawTitle(canvas, title,
              TitleBox{layout.centreX(), layout.titleCapTop(), layout.titleCapHeight(), layout.dp(),
                       true},
              style);
}

} // namespace opensu::ui

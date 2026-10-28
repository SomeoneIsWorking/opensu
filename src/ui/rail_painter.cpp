#include "rail_painter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "round_shape.hpp"

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

// library.svg's edge is its body's bottom colour at about this brightness.
constexpr float edgeShade = 0.66f;

Colour shaded(Colour colour, float by) noexcept {
    const auto scale = [by](std::uint8_t channel) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(channel) * by));
    };
    return Colour{scale(colour.r), scale(colour.g), scale(colour.b), colour.a};
}

} // namespace

const std::vector<ColourSwap>& RailPainter::swapsFor(const Focused& focused) {
    std::string key = focused.key;
    if (key.empty() && focused.platform != nullptr) {
        key = focused.platform->key;
    }
    if (const auto found = swaps_.find(key); found != swaps_.end()) {
        return found->second;
    }
    std::vector<ColourSwap> swaps;
    if (focused.platform != nullptr) {
        const Colour top = unpack(focused.platform->strokeFrom);
        const Colour bottom = unpack(focused.platform->strokeTo);
        swaps = {ColourSwap{libraryTop, top}, ColourSwap{libraryBottom, bottom},
                 ColourSwap{libraryEdge, shaded(bottom, edgeShade)}};
    }
    return swaps_.emplace(key, std::move(swaps)).first->second;
}

void RailPainter::paintColumn(Canvas& canvas, const XmbLayout& layout, const Focused& focused,
                              const RailStyle& style) {
    const Rect& box = layout.sectionIcon();
    const int width = std::max(1, static_cast<int>(std::lround(box.width)));
    const int height = std::max(1, static_cast<int>(std::lround(box.height)));
    const std::vector<ColourSwap>& swaps = swapsFor(focused);
    if (const gpu2d::Texture* icon = icons_.image(Icon::Library, width, height, swaps)) {
        canvas.image(icon->view(), wholeOf(*icon),
                     gpu2d::Rect{std::round(box.x), std::round(box.y), static_cast<float>(width),
                                 static_cast<float>(height)},
                     withAlpha(white, style.alpha));
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
    const float size = box.capHeight / capHeightPerEm;
    const float left = box.centred ? box.x - text_.measure(title, size) * 0.5f : box.x;
    const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(shadowTaps);
    for (int tap = 0; tap < shadowTaps; ++tap) {
        const float angle = step * static_cast<float>(tap);
        text_.drawFromCapTop(
            canvas, title,
            Point{left + (shadowOffsetDp + shadowRadiusDp * std::cos(angle)) * box.dp,
                  box.capTop + (shadowOffsetDp + shadowRadiusDp * std::sin(angle)) * box.dp},
            size, withAlpha(black, shadowAlpha * style.alpha));
    }
    text_.drawFromCapTop(canvas, title, Point{left, box.capTop}, size,
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

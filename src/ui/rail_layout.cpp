#include "rail_layout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include "tile_motion.hpp"

namespace opensu::ui {
namespace {

// iiSU e39: the vertical XMB's proportions of the canvas. The slot's left edge is measured, 340.5
// px on 1920 (navigation.md §5.3), where `0.185 W` gives 355.
constexpr float xmbLeftMargin = 340.5f / 1920.0f;
constexpr float xmbFocused = 0.30f;
constexpr float xmbUnfocused = 0.15f;
constexpr float xmbGap = 0.08f;
constexpr float xmbCentre = 0.5f;
// iiSU lz3: the horizontal path's.
constexpr float carouselFocused = 0.30f;
constexpr float carouselUnfocused = 0.20f;
constexpr float carouselGap = 0.15f;
constexpr float carouselCentreX = 0.5f;
constexpr float carouselBaseline = 0.85f;
// Measured on 1920 x 1080 at 2.25 px per dp (navigation.md §5.3, §5.4): the left column's icon
// (74 x 44 dp) and the marker's centres, the console card (72 dp), the title's offset from the
// slot and its capital height (58 px), and the Carousel's title and marker.
constexpr float xmbColumnX = 187.0f / 1920.0f;
constexpr float xmbHeaderX = 188.5f / 1920.0f;
constexpr float xmbMarkerX = 313.5f / 1920.0f;
constexpr float sectionIconWidthDp = 74.0f;
constexpr float sectionIconHeightDp = 44.0f;
constexpr float headerCardDp = 72.0f;
constexpr float xmbTitleGapDp = 17.1f;
constexpr float xmbTitleCapOffsetDp = 16.4f;
constexpr float xmbTitleCapDp = 25.8f;
constexpr float carouselTitleCapTop = 364.0f / 1080.0f;
constexpr float carouselTitleCapDp = 12.9f;
constexpr float carouselMarkerY = 946.0f / 1080.0f;
// The level the horizontal path's scale is taken against (lz3: k(L) / k(10)).
constexpr int carouselReferenceLevel = 10;

/// A tile's size at a fractional focus (iiSU `e39`, `lz3`): the two sizes blended by a smoothstep
/// of how near the tile is to the focus.
struct SizePair {
    float small;
    float large;
};

float blend(SizePair sizes, std::size_t index, float focus) noexcept {
    const float near = 1.0f - std::clamp(std::abs(static_cast<float>(index) - focus), 0.0f, 1.0f);
    return sizes.small + (sizes.large - sizes.small) * motion::smoothstep(near);
}

/// The centres of the tiles placed along an axis, `gap` apart, with the tile at `focus` at
/// `anchor`. A tile between two positions is laid out for each neighbour in turn and the two
/// layouts blended, so the row moves continuously.
struct Anchor {
    float centre;
    float gap;
};

/// The tiles placed: `first` and the centres from it on.
struct Stack {
    std::size_t first{};
    std::vector<float> centres;
};

/// What `stack` lays out: `count` tiles of `extent(i)` along an axis `canvas` long.
struct Strip {
    std::size_t count;
    float canvas;
    const std::function<float(std::size_t)>& extent;
};

/// The tiles from `pivot` outward, anchored on it, that reach within half a canvas of the canvas.
std::pair<std::size_t, std::size_t> reach(const Strip& strip, Anchor anchor, std::size_t pivot) {
    const float pad = strip.canvas * 0.5f;
    std::size_t low = pivot;
    std::size_t high = pivot;
    float centre = anchor.centre;
    while (high + 1 < strip.count && centre + strip.extent(high) * 0.5f < strip.canvas + pad) {
        centre += (strip.extent(high) + strip.extent(high + 1)) * 0.5f + anchor.gap;
        ++high;
    }
    centre = anchor.centre;
    while (low > 0 && centre - strip.extent(low) * 0.5f > -pad) {
        centre -= (strip.extent(low) + strip.extent(low - 1)) * 0.5f + anchor.gap;
        --low;
    }
    return {low, high};
}

Stack stack(const Strip& strip, Anchor anchor, float focus) {
    if (strip.count == 0) {
        return {};
    }
    const auto lower = static_cast<std::size_t>(std::floor(focus));
    const auto upper = static_cast<std::size_t>(std::ceil(focus));
    const float mix = focus - std::floor(focus);
    const auto [lowA, highA] = reach(strip, anchor, lower);
    const auto [lowB, highB] = reach(strip, anchor, upper);
    const std::size_t first = std::min(lowA, lowB);
    const std::size_t last = std::max(highA, highB);
    std::vector<float> extents;
    extents.reserve(last - first + 1);
    for (std::size_t i = first; i <= last; ++i) {
        extents.push_back(strip.extent(i));
    }
    const auto anchored = [&](std::size_t pivot) {
        std::vector<float> out(extents.size(), anchor.centre);
        const std::size_t at = pivot - first;
        for (std::size_t i = at + 1; i < out.size(); ++i) {
            out[i] = out[i - 1] + (extents[i - 1] + extents[i]) * 0.5f + anchor.gap;
        }
        for (std::size_t i = at; i-- > 0;) {
            out[i] = out[i + 1] - (extents[i] + extents[i + 1]) * 0.5f - anchor.gap;
        }
        return out;
    };
    const std::vector<float> below = anchored(lower);
    const std::vector<float> above = anchored(upper);
    Stack placed{first, std::vector<float>(extents.size())};
    for (std::size_t i = 0; i < extents.size(); ++i) {
        placed.centres[i] = below[i] + (above[i] - below[i]) * mix;
    }
    return placed;
}

float clampedFocus(const RailInput& input) noexcept {
    const auto last = static_cast<float>(input.count == 0 ? 0 : input.count - 1);
    return std::clamp(input.focus, 0.0f, last);
}

float aspectOf(const RailInput& input, std::size_t index) {
    const float aspect = input.aspect ? input.aspect(index) : 1.0f;
    return aspect > 0.0f ? aspect : 1.0f;
}

} // namespace

float iconScale(int level) noexcept {
    return std::clamp((static_cast<float>(std::clamp(level, 1, 20)) - 10.0f) * 0.11f + 1.45f, 0.67f,
                      2.55f);
}

XmbLayout::XmbLayout(const RailInput& input) : focus_{clampedFocus(input)}, dp_{input.dp} {
    const float k = iconScale(input.iconLevel);
    leftMargin_ = xmbLeftMargin * input.width;
    focusedSize_ = xmbFocused * input.height * k;
    unfocusedSize_ = xmbUnfocused * input.height * k;
    gap_ = xmbGap * unfocusedSize_;
    centreY_ = xmbCentre * input.height;
    sectionIcon_ = Rect{xmbColumnX * input.width - sectionIconWidthDp * input.dp * 0.5f,
                        centreY_ - sectionIconHeightDp * input.dp * 0.5f,
                        sectionIconWidthDp * input.dp, sectionIconHeightDp * input.dp};
    headerCard_ = Rect{xmbHeaderX * input.width - headerCardDp * input.dp * 0.5f,
                       centreY_ - headerCardDp * input.dp * 0.5f, headerCardDp * input.dp,
                       headerCardDp * input.dp};
    markerX_ = xmbMarkerX * input.width;
    titleLeft_ = leftMargin_ + focusedSize_ + xmbTitleGapDp * input.dp;
    titleCapTop_ = centreY_ - focusedSize_ * 0.5f + xmbTitleCapOffsetDp * input.dp;
    titleCapHeight_ = xmbTitleCapDp * input.dp;

    // A tile wider than it is tall shrinks to the column's width (iiSU `w70.V`); a taller one
    // keeps the column's height and is as narrow as its art.
    const std::function<float(std::size_t)> height = [&](std::size_t i) {
        return sizeOf(i) / std::max(aspectOf(input, i), 1.0f);
    };
    const Stack placed =
        stack(Strip{input.count, input.height, height}, Anchor{centreY_, gap_}, focus_);
    first_ = placed.first;
    for (std::size_t i = 0; i < placed.centres.size(); ++i) {
        const std::size_t tile = placed.first + i;
        const float tall = height(tile);
        const float width = sizeOf(tile) * std::min(aspectOf(input, tile), 1.0f);
        rects_.push_back(Rect{leftMargin_, placed.centres[i] - tall * 0.5f, width, tall});
    }
}

float XmbLayout::sizeOf(std::size_t index) const noexcept {
    return blend(SizePair{unfocusedSize_, focusedSize_}, index, focus_);
}

CarouselLayout::CarouselLayout(const RailInput& input)
    : focus_{clampedFocus(input)}, dp_{input.dp} {
    const float k = iconScale(input.iconLevel) / iconScale(carouselReferenceLevel);
    centreX_ = carouselCentreX * input.width;
    baselineY_ = carouselBaseline * input.height;
    focusedSize_ = carouselFocused * input.height * k;
    unfocusedSize_ = carouselUnfocused * input.height * k;
    gap_ = carouselGap * unfocusedSize_;
    titleCapTop_ = carouselTitleCapTop * input.height;
    titleCapHeight_ = carouselTitleCapDp * input.dp;
    markerY_ = carouselMarkerY * input.height;

    const std::function<float(std::size_t)> width = [&](std::size_t i) {
        return sizeOf(i) * std::clamp(aspectOf(input, i), narrowestSlot, widestSlot);
    };
    const Stack placed =
        stack(Strip{input.count, input.width, width}, Anchor{centreX_, gap_}, focus_);
    first_ = placed.first;
    for (std::size_t i = 0; i < placed.centres.size(); ++i) {
        const std::size_t tile = placed.first + i;
        const float wide = width(tile);
        const float height = sizeOf(tile);
        rects_.push_back(Rect{placed.centres[i] - wide * 0.5f, baselineY_ - height, wide, height});
    }
}

float CarouselLayout::sizeOf(std::size_t index) const noexcept {
    return blend(SizePair{unfocusedSize_, focusedSize_}, index, focus_);
}

std::optional<std::size_t> railTileAt(const std::vector<Rect>& tiles, std::size_t first,
                                      float focus, float x, float y) {
    std::optional<std::size_t> found;
    float best = 0.0f;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const float distance = std::abs(static_cast<float>(first + i) - focus);
        if (tiles[i].contains(x, y) && (!found || distance < best)) {
            found = first + i;
            best = distance;
        }
    }
    return found;
}

} // namespace opensu::ui

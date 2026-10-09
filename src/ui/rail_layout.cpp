#include "rail_layout.hpp"

#include <algorithm>
#include <cmath>

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

/// The centres of tiles of `extents` along an axis, `gap` apart, with the tile at `focus` at
/// `anchor`. A tile between two positions is laid out for each neighbour in turn and the two
/// layouts blended, so the row moves continuously.
struct Anchor {
    float centre;
    float gap;
};

std::vector<float> stack(const std::vector<float>& extents, Anchor anchor, float focus) {
    const std::size_t count = extents.size();
    std::vector<float> centres(count, anchor.centre);
    if (count == 0) {
        return centres;
    }
    const auto lower = static_cast<std::size_t>(std::floor(focus));
    const auto upper = static_cast<std::size_t>(std::ceil(focus));
    const float mix = focus - std::floor(focus);
    const auto anchored = [&](std::size_t pivot) {
        std::vector<float> out(count, anchor.centre);
        for (std::size_t i = pivot + 1; i < count; ++i) {
            out[i] = out[i - 1] + (extents[i - 1] + extents[i]) * 0.5f + anchor.gap;
        }
        for (std::size_t i = pivot; i-- > 0;) {
            out[i] = out[i + 1] - (extents[i] + extents[i + 1]) * 0.5f - anchor.gap;
        }
        return out;
    };
    const std::vector<float> below = anchored(lower);
    const std::vector<float> above = anchored(upper);
    for (std::size_t i = 0; i < count; ++i) {
        centres[i] = below[i] + (above[i] - below[i]) * mix;
    }
    return centres;
}

float clampedFocus(const RailInput& input) noexcept {
    const auto last = static_cast<float>(input.aspects.empty() ? 0 : input.aspects.size() - 1);
    return std::clamp(input.focus, 0.0f, last);
}

float aspectOf(float aspect) noexcept {
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
    std::vector<float> heights;
    heights.reserve(input.aspects.size());
    for (std::size_t i = 0; i < input.aspects.size(); ++i) {
        heights.push_back(sizeOf(i) / std::max(aspectOf(input.aspects[i]), 1.0f));
    }
    const std::vector<float> centres = stack(heights, Anchor{centreY_, gap_}, focus_);
    for (std::size_t i = 0; i < heights.size(); ++i) {
        const float width = sizeOf(i) * std::min(aspectOf(input.aspects[i]), 1.0f);
        rects_.push_back(Rect{leftMargin_, centres[i] - heights[i] * 0.5f, width, heights[i]});
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

    std::vector<float> widths;
    widths.reserve(input.aspects.size());
    for (std::size_t i = 0; i < input.aspects.size(); ++i) {
        widths.push_back(sizeOf(i) *
                         std::clamp(aspectOf(input.aspects[i]), narrowestSlot, widestSlot));
    }
    const std::vector<float> centres = stack(widths, Anchor{centreX_, gap_}, focus_);
    for (std::size_t i = 0; i < widths.size(); ++i) {
        const float height = sizeOf(i);
        rects_.push_back(
            Rect{centres[i] - widths[i] * 0.5f, baselineY_ - height, widths[i], height});
    }
}

float CarouselLayout::sizeOf(std::size_t index) const noexcept {
    return blend(SizePair{unfocusedSize_, focusedSize_}, index, focus_);
}

} // namespace opensu::ui

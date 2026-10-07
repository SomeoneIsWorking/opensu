#include "top_bar_metrics.hpp"

#include <algorithm>
#include <cmath>

namespace iideck::ui {
namespace {

/// coerceIn, which is what iiSU's gk2.C is.
float clampTo(float value, float low, float high) noexcept {
    return std::min(std::max(value, low), high);
}

// Material 3 titleMedium is 16 sp; iiSU a32.p scales it by the content scale c.
constexpr float titleMediumSp = 16.0f;
// iiSU mw5.l passes 9 as a32.o's glyph x base (p9).
constexpr float glyphBaseX = 9.0f;
// STOPGAP: jj2.b's scale argument from mw5.h is not traced, so the hint row uses scale 1.
constexpr float hintScale = 1.0f;

} // namespace

TopBarMetrics::TopBarMetrics(float screenWidthDp) noexcept {
    // iiSU jj2.k0: t = clamp((max(w, 360) - 360) / 472, 0, 1).
    const float t = clampTo((std::max(screenWidthDp, 360.0f) - 360.0f) / 472.0f, 0.0f, 1.0f);
    sizing_.scale = 0.72f + 0.38f * t;
    sizing_.stretch = (0.62f + 0.38f * t) * 0.85f;
    sizing_.endPadding = 8.0f + 8.0f * t;

    // iiSU pl3.q: hs7 from s = clamp(is7.a, 0.7, 1.6) and k = clamp(0.78 s, 0.55, 1.6).
    const float s = clampTo(sizing_.scale, 0.7f, 1.6f);
    const float k = clampTo(0.78f * s, 0.55f, 1.6f);
    alignment_.statusPillHeight = clampTo(48.0f * k, 28.0f, 86.0f);
    alignment_.statusTopPadding = clampTo(10.0f * k, 4.0f, 16.0f);
    alignment_.profileTopPadding = std::max(0.0f, (alignment_.statusPillHeight - 62.0f) / 2.0f +
                                                      alignment_.statusTopPadding + 8.0f);
    alignment_.compactWidthScale = clampTo(0.546f / s * 1.15f, 0.45f, 0.82f);
}

float TopBarMetrics::statusOffsetX(float aspect) const noexcept {
    // iiSU mw5.l: r is how close the window is to 4:3.
    const float r = clampTo(1.0f - std::abs(aspect - 4.0f / 3.0f) / 0.12f, 0.0f, 1.0f);
    const float a = sizing_.scale;
    return 6.0f * r + 4.0f * clampTo((a - 1.04f) / 0.06f, 0.0f, 1.0f) +
           16.0f * clampTo((1.02f - a) / 0.3f, 0.0f, 1.0f);
}

float TopBarMetrics::profileSize() const noexcept {
    return clampTo(1.22f * alignment_.statusPillHeight, 38.0f, 64.0f);
}

float TopBarMetrics::gridTopInset() const noexcept {
    const float c = alignment_.statusTopPadding;
    const float d = alignment_.statusPillHeight;
    const float profile = profileSize();
    return std::max(c + 8.0f + d, std::max((d - profile) / 2.0f + c + 8.0f, 4.0f) + profile) + 3.0f;
}

StatusPillMetrics TopBarMetrics::statusPill(bool clockHasLetters) const noexcept {
    StatusPillMetrics pill;
    // iiSU a32.o: s from is7.a, k = clamp(0.78 s, 0.5, 1.6), c from is7.b.
    const float s = clampTo(sizing_.scale, 0.7f, 1.6f);
    const float k = clampTo(clampTo(0.78f, 0.6f, 1.2f) * s, 0.5f, 1.6f);
    const float c = clampTo(std::min(clampTo(sizing_.stretch, 0.55f, 1.2f), 1.0f) * s, 0.42f, 1.6f);
    pill.contentScale = c;
    pill.height = clampTo(48.0f * k, 30.0f, 86.0f);
    pill.width =
        clockHasLetters ? clampTo(228.0f * s, 188.0f, 320.0f) : clampTo(204.0f * s, 168.0f, 292.0f);
    pill.bellColumn = clampTo(46.0f * c, 14.0f, 86.0f);
    pill.ringSize = clampTo(22.0f * c, 10.0f, 28.0f);
    pill.textSpacing = clampTo(5.0f * c, 2.0f, 10.0f);
    pill.fontSize = std::max(titleMediumSp * c, 10.0f);
    pill.batteryIcon = clampTo(27.0f * c, 12.0f, 42.0f);
    pill.glyphSize = clampTo(28.0f * k, 18.0f, 40.0f);
    const float wide = clampTo((s - 0.84f) / 0.28f, 0.0f, 1.0f);
    pill.glyphOffsetX =
        clockHasLetters
            ? clampTo(clampTo(4.0f * s, 2.0f, 12.0f) + glyphBaseX - wide * 4.0f, 0.0f, 26.0f)
            : clampTo(clampTo(8.0f * s, 4.0f, 18.0f) + glyphBaseX - wide * 4.0f, 0.0f, 32.0f);
    pill.glyphOffsetY = clampTo(-9.0f * k, -14.0f, 8.0f);
    return pill;
}

HintRowMetrics TopBarMetrics::hintRow() const noexcept {
    HintRowMetrics row;
    // iiSU mw5.h: BottomStart, padding start 8, bottom 2 in the single-screen layout.
    row.paddingStart = 8.0f;
    row.paddingBottom = 2.0f;
    // iiSU jj2.b: f = clamp(scale, 0.65, 1.1).
    const float f = clampTo(hintScale, 0.65f, 1.1f);
    row.paddingHorizontal = clampTo(10.0f * f, 6.0f, 10.0f);
    row.paddingVertical = clampTo(8.0f * f, 5.0f, 7.0f);
    row.entrySpacing = clampTo(2.0f * f, 1.0f, 3.0f);
    // iiSU a32.b: glyph to label gap clamp(4 f, 2, 6) with f = clamp(scale, 0.65, 1.2).
    row.glyphGap = clampTo(4.0f * clampTo(hintScale, 0.65f, 1.2f), 2.0f, 6.0f);
    return row;
}

} // namespace iideck::ui

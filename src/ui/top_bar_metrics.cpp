#include "top_bar_metrics.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

/// coerceIn, which is what iiSU's gk2.C is.
float clampTo(float value, float low, float high) noexcept {
    return std::min(std::max(value, low), high);
}

// Material 3 titleMedium is 16 sp; iiSU a32.p scales it by the content scale c.
constexpr float titleMediumSp = 16.0f;
// iiSU pl3.q: dl3.d is typography slot 8 (Material 3 titleMedium, 16 sp on 24 sp) with Cal Sans's
// pp4.g overrides (size x1, line x1.08, 0.03 em), scaled by dl3.b and its line by a further 0.92.
constexpr float labelSizeSp = 16.0f;
constexpr float labelLineSp = 24.0f * 1.08f;
constexpr float labelLineScale = 0.92f;
constexpr float labelTrackingEm = 0.03f;
// iiSU pp4.e under Cal Sans: -(clamp(t, 0.5, 1.6) * 1.8).
constexpr float calSansNudge = 1.8f;
// iiSU mw5.l p11: the entries' base text offset; jj2.p0 lowers it by 0.65 for +, - and X.
constexpr float entryOffset = 1.5f;
constexpr float signOffset = 0.65f;

} // namespace

TopBarMetrics::TopBarMetrics(float screenWidthDp, float screenHeightDp) noexcept {
    // iiSU jj2.f0: the screen against 853 x 480 dp; jj2.i0 marks small screens compact.
    const float f0 = clampTo(
        std::min(std::min(screenWidthDp / 853.0f, 1.0f), std::min(screenHeightDp / 480.0f, 1.0f)) *
            0.92f,
        0.68f, 1.0f);
    const bool compact = std::min(screenWidthDp, screenHeightDp) <= 420.0f &&
                         std::max(screenWidthDp, screenHeightDp) <= 560.0f;
    // iiSU pl3.q: dl3.a.
    promptScale_ = compact ? clampTo(f0 * 0.9f, 0.65f, 1.0f) : clampTo(f0 * 0.94f, 0.68f, 1.0f);
    compact_ = compact;
    widthDp_ = screenWidthDp;

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

    // iiSU pl3.q: dl3.b from jj2.j0(is7), dl3.c from dl3.b.
    const float j0 =
        clampTo(s * std::min(clampTo(sizing_.stretch, 0.55f, 1.2f), 1.0f), 0.42f, 1.6f);
    textScale_ = clampTo(j0 * 0.94f, compact ? 0.6f : 0.42f, 1.6f);
    glyphScale_ = clampTo(textScale_ * 0.9f, compact ? 0.54f : 0.42f, 1.2f);
}

float TopBarMetrics::statusOffsetX(float aspect) const noexcept {
    // iiSU mw5.l: r is how close the window is to 4:3.
    const float r = clampTo(1.0f - std::abs(aspect - 4.0f / 3.0f) / 0.12f, 0.0f, 1.0f);
    const float a = sizing_.scale;
    return 6.0f * r + 4.0f * clampTo((a - 1.04f) / 0.06f, 0.0f, 1.0f) +
           16.0f * clampTo((1.02f - a) / 0.3f, 0.0f, 1.0f);
}

TitlePillMetrics TopBarMetrics::titlePill() const noexcept {
    // f = 0.9 + 0.1 clamp((max(w, 538) - 538) / 294).
    const float f =
        0.9f + 0.1f * clampTo((std::max(widthDp_, 538.0f) - 538.0f) / 294.0f, 0.0f, 1.0f);
    TitlePillMetrics pill;
    pill.topPadding = alignment_.statusTopPadding;
    pill.height = clampTo(48.0f * f, 36.0f, 56.0f);
    pill.paddingHorizontal = clampTo(22.0f * f, 12.0f, 22.0f);
    pill.minWidth = clampTo(140.0f * f, 110.0f, 160.0f);
    pill.maxTextWidth = 520.0f - 2.0f * pill.paddingHorizontal;
    pill.fontSize = std::max(titleMediumSp * clampTo(f, 0.72f, 1.0f), 11.0f);
    return pill;
}

float TopBarMetrics::avatarSize() noexcept {
    return clampTo(55.0f * 0.7f * 0.82f, 24.0f, 34.0f);
}

float TopBarMetrics::profileSize() const noexcept {
    return clampTo(1.22f * alignment_.statusPillHeight, 38.0f, 64.0f);
}

float TopBarMetrics::promptRowHeight(float scale) noexcept {
    // iiSU jj2.q0: two glyph rows, their vertical padding and the spacing between them.
    const float f = clampTo(scale, 0.65f, 1.1f);
    return 2.0f * clampTo(22.0f * f, 15.0f, 24.0f) + 2.0f * clampTo(8.0f * f, 5.0f, 7.0f) +
           clampTo(2.0f * f, 1.0f, 3.0f);
}

float TopBarMetrics::gridTopInset() const noexcept {
    const float c = alignment_.statusTopPadding;
    const float d = alignment_.statusPillHeight;
    const float profile = profileSize();
    const float hud =
        std::max(c + 8.0f + d, std::max((d - profile) / 2.0f + c + 8.0f, 4.0f) + profile) + 3.0f;
    return hud + 4.0f;
}

float TopBarMetrics::gridBottomInset() const noexcept {
    // iiSU pl3.q: dl3.j = jj2.q0(dl3.a) + 14 while the prompt row shows.
    return promptRowHeight(promptScale_) + 14.0f + 4.0f;
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
    pill.paddingHorizontal = clampTo(10.0f * c, 6.0f, 16.0f);
    pill.textSpacing = clampTo(5.0f * c, 2.0f, 10.0f);
    pill.fontSize = std::max(titleMediumSp * c, 10.0f);
    pill.batteryIcon = clampTo(27.0f * c, 12.0f, 42.0f);
    return pill;
}

HintPanelMetrics TopBarMetrics::hintPanels() const noexcept {
    HintPanelMetrics panel;
    // iiSU mw5.h: BottomStart, start 8, bottom 2; mw5.i/j: BottomEnd, end 8, bottom 4.
    panel.leftInset = 8.0f;
    panel.leftBottom = 2.0f;
    panel.rightInset = 8.0f;
    panel.rightBottom = 4.0f;
    // iiSU mw5.l hands jj2.b dl3.c(), dl3.a(), dl3.b(): the panel, text and glyph scales.
    const float f = clampTo(promptScale_, 0.65f, 1.1f);
    panel.paddingHorizontal = clampTo(10.0f * f, 6.0f, 10.0f);
    panel.paddingVertical =
        compact_ ? clampTo(6.0f * f, 3.0f, 5.0f) : clampTo(8.0f * f, 5.0f, 7.0f);
    panel.entrySpacing = clampTo(2.0f * f, 1.0f, 3.0f);
    panel.glyphSize = clampTo(22.0f * clampTo(glyphScale_, 0.65f, 1.2f), 15.0f, 24.0f);
    const float text = clampTo(textScale_, 0.65f, 1.2f);
    panel.glyphGap = clampTo(4.0f * text, 2.0f, 6.0f);
    // iiSU jj2.d: RoundedCornerShape(8 dp) by default.
    panel.cornerRadius = 8.0f;
    panel.labelSize = labelSizeSp * textScale_;
    panel.labelLineHeight = labelLineSp * textScale_ * labelLineScale;
    panel.labelTracking = labelTrackingEm;
    panel.labelNudge = -clampTo(text, 0.5f, 1.6f) * calSansNudge;
    const float floorScale = clampTo(f / 0.94f, 0.65f, 1.1f);
    panel.minHeight = promptRowHeight(floorScale) - clampTo(4.0f * floorScale, 2.0f, 4.0f);
    return panel;
}

float HintPanelMetrics::height(float density) const noexcept {
    // iiSU jj2.g0: spacing + 2 max(glyph, text offset + "Ag" height) + 2 vertical padding, in
    // whole pixels; Compose rounds the line height up.
    const auto px = [density](float dp) {
        return std::round(dp * density);
    };
    const float text =
        px(std::max(entryOffset + labelNudge, 0.0f)) + std::ceil(labelLineHeight * density);
    const float content =
        px(entrySpacing) + 2.0f * std::max(px(glyphSize), text) + 2.0f * px(paddingVertical);
    return std::max(content / density, minHeight);
}

float HintPanelMetrics::labelShift(std::string_view key) const noexcept {
    // a32.b pads the label's top by a positive offset, which the centred row halves, and moves
    // it by a negative one.
    const bool sign = key == "+" || key == "-" || key == "X";
    const float offset = entryOffset - (sign ? signOffset : 0.0f) + labelNudge;
    return offset > 0.0f ? offset * 0.5f : offset;
}

} // namespace opensu::ui

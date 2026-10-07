#include "hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "clock_text.hpp"
#include "typeface.hpp"

namespace iideck::ui {
namespace {

// iiSU mw5.h: ("B", "Back") and ("-", "Details"). iideck has no Back action, so the first entry
// is its primary action; Select opens details, as "-" does in iiSU.
constexpr std::array<std::pair<const char*, const char*>, 2> hints{
    std::pair{"A", "Play"},
    std::pair{"-", "Details"},
};
// STOPGAP: hint text and glyph sizes follow the status pill's font because a32.b's text style
// from mw5.h is not in the spec.
constexpr float hintGlyphScale = 1.3f;
// iiSU res/drawable/bell_icon.png ink, for the hint glyphs and labels.
constexpr Color hintInk{0x4D, 0x46, 0x55, 255};

} // namespace

void Hud::setSize(int width, int height, float dp) noexcept {
    width_ = width;
    height_ = height;
    dp_ = dp;
}

float Hud::unit() const noexcept {
    return static_cast<float>(std::min(width_, height_)) / 100.0f;
}

TopBarMetrics Hud::metrics() const noexcept {
    return TopBarMetrics{static_cast<float>(width_) / dp_, static_cast<float>(height_) / dp_};
}

float Hud::topInset() const noexcept {
    return metrics().gridTopInset() * dp_;
}

float Hud::bottomInset() const noexcept {
    return metrics().gridBottomInset() * dp_;
}

void Hud::setToast(std::string text, bool isError, Clock::time_point now) {
    toast_ = std::move(text);
    toastError_ = isError;
    toastUntil_ = now + toastLifetime;
}

void Hud::tick(Clock::time_point now) {
    if (!toast_.empty() && now >= toastUntil_) {
        toast_.clear();
    }
}

void Hud::drawGround() const {
    ClearBackground(palette::ground);
    const float u = unit();
    const float spacing = std::max(u * 2.2f, 8.0f);
    const float radius = std::max(spacing * 0.09f, 1.0f);
    const Color dot{palette::ink.r, palette::ink.g, palette::ink.b, 24};
    for (float y = spacing; y < static_cast<float>(height_); y += spacing) {
        for (float x = spacing; x < static_cast<float>(width_); x += spacing) {
            DrawCircleV({x, y}, radius, dot);
        }
    }
}

void Hud::drawTopBar() const {
    // iiSU mw5.l single-screen Row: top 8, end 8, friends | title (weight 1) | status, all Top.
    const TopBarMetrics m = metrics();
    const float dp = dp_;
    const float width = static_cast<float>(width_);
    const float top = TopBarMetrics::rowPaddingTop * dp;
    const StatusPillMetrics pill = m.statusPill(ClockText::hasLetters(clock_));
    const float aspect = std::max(width, static_cast<float>(height_)) /
                         std::max(std::min(width, static_cast<float>(height_)), 1.0f);
    // The status box ends at the row's end padding and is offset by the device class rule.
    const float boxRight = width - TopBarMetrics::rowPaddingEnd * dp + m.statusOffsetX(aspect) * dp;
    const Rect body{boxRight - (m.sizing().endPadding + pill.width) * dp, top, pill.width * dp,
                    pill.height * dp};
    statusPill_.paint(StatusPillView{body, pill, dp, clock_, battery_});
    // STOPGAP: jj2.w's glass strip behind the status pill is not drawn because its geometry is
    // not in the spec.

    // iideck has no friends, so the friends slot is empty; the Steam indicator, iideck's own,
    // sits there, at the row's end padding from the left edge.
    drawServiceStatus(TopBarMetrics::rowPaddingEnd * dp, body.centreY());
}

void Hud::drawServiceStatus(float left, float centreY) const {
    if (steamState_ == ServiceState::Hidden) {
        return;
    }
    const float u = unit();
    Color dot = palette::dotReady;
    const char* label = "ready";
    switch (steamState_) {
    case ServiceState::Starting:
        dot = palette::dotWorking;
        label = "starting";
        break;
    case ServiceState::Failed:
        dot = palette::dotFailed;
        label = "failed";
        break;
    case ServiceState::Blocked:
        dot = palette::dotFailed;
        label = "on desktop";
        break;
    case ServiceState::Hidden:
    case ServiceState::Ready:
        break;
    }

    // A generic client glyph (a ring with a dot inside), the state dot, then the label.
    const float glyphRadius = u * 0.9f;
    const Vector2 glyph{left + glyphRadius, centreY};
    DrawRing(glyph, glyphRadius * 0.78f, glyphRadius, 0.0f, 360.0f, 32, palette::inkSoft);
    DrawCircleV(glyph, glyphRadius * 0.34f, palette::inkSoft);
    const float dotRadius = u * 0.35f;
    const float dotX = glyph.x + glyphRadius + u * 0.7f + dotRadius;
    DrawCircleV({dotX, centreY}, dotRadius, dot);
    const int size = static_cast<int>(u * 1.5f);
    type().draw(label, dotX + dotRadius + u * 0.6f, centreY - static_cast<float>(size) / 2.0f, size,
                palette::inkSoft);
}

void Hud::drawHints() const {
    // iiSU mw5.h: a glass row at BottomStart holding jj2.b's entries.
    const TopBarMetrics m = metrics();
    const HintRowMetrics row = m.hintRow();
    const float dp = dp_;
    const int size = static_cast<int>(std::lround(m.statusPill(false).fontSize * dp));
    const float glyph = static_cast<float>(size) * hintGlyphScale;
    const float gap = row.glyphGap * dp;
    float content = 0.0f;
    for (const auto& [key, label] : hints) {
        content += glyph + gap + type().measure(label, size);
    }
    content += static_cast<float>(hints.size() - 1) * row.entrySpacing * dp;
    const float height = glyph + 2.0f * row.paddingVertical * dp;
    const Rect body{row.paddingStart * dp,
                    static_cast<float>(height_) - row.paddingBottom * dp - height,
                    content + 2.0f * row.paddingHorizontal * dp, height};
    glass_.paint(body);

    float pen = body.x + row.paddingHorizontal * dp;
    const float centreY = body.centreY();
    for (const auto& [key, label] : hints) {
        DrawCircleV(Vector2{pen + glyph * 0.5f, centreY}, glyph * 0.5f, hintInk);
        const float keyWidth = type().measure(key, size);
        type().draw(key, pen + (glyph - keyWidth) * 0.5f, centreY - static_cast<float>(size) * 0.5f,
                    size, WHITE);
        pen += glyph + gap;
        type().draw(label, pen, centreY - static_cast<float>(size) * 0.5f, size, hintInk);
        pen += type().measure(label, size) + row.entrySpacing * dp;
    }
}

void Hud::drawToast() const {
    if (toast_.empty()) {
        return;
    }
    const float u = unit();
    const int size = static_cast<int>(u * 1.4f);
    const float textWidth = type().measure(toast_, size);
    const float padding = u * 2.0f;
    const Rectangle box{
        (static_cast<float>(width_) - textWidth - 2.0f * padding) / 2.0f,
        static_cast<float>(height_) - u * 8.0f,
        textWidth + 2.0f * padding,
        static_cast<float>(size) + padding,
    };
    DrawRectangleRounded(box, 0.5f, 12,
                         toastError_ ? Color{0xb3, 0x26, 0x1e, 240} : Color{0x2b, 0x27, 0x33, 240});
    type().draw(toast_.c_str(), box.x + padding / 2.0f, box.y + padding / 3.0f, size, WHITE);
}

} // namespace iideck::ui

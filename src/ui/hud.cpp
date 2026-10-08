#include "hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include "clock_text.hpp"
#include "typeface.hpp"

namespace iideck::ui {
namespace {

using Prompt = std::pair<const char*, const char*>;
// iiSU mw5.h: ("B", "Back"), ("-", "Details").
constexpr std::array<Prompt, 2> leftPrompts{Prompt{"B", "Back"}, Prompt{"-", "Details"}};
// iiSU mw5.k: ("A", "Select"), ("+", "Menu"); iideck's START opens a menu in Library only.
constexpr std::array<Prompt, 1> rightPrompts{Prompt{"A", "Select"}};
constexpr std::array<Prompt, 2> rightPromptsWithMenu{Prompt{"A", "Select"}, Prompt{"+", "Menu"}};
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
    now_ = now;
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

void Hud::drawTopBar() {
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
    const double seconds = std::chrono::duration<double>(now_.time_since_epoch()).count();
    std::optional<double> busy;
    if (std::ranges::any_of(launchers_, [](const LauncherBadge& badge) {
            return badge.progress.has_value();
        })) {
        busy = seconds;
    }
    statusPill_.paint(StatusPillView{body, pill, dp, clock_, battery_, busy});
    // STOPGAP: jj2.w's glass strip behind the status pill is not drawn because its geometry is
    // not in the spec.

    drawTitlePill(top);

    // iiSU a32.e lays friends' avatars in this slot; iideck has no friends, so it holds the
    // launchers' badges, spaced rather than overlapped so each state reads on its own.
    const float avatar = TopBarMetrics::avatarSize() * dp;
    badges_.paint(launchers_, BadgeRow{.start = {TopBarMetrics::rowPaddingEnd * dp, body.centreY()},
                                       .diameter = avatar,
                                       .gap = avatar * 0.3f,
                                       .seconds = seconds});
}

void Hud::drawTitlePill(float top) const {
    if (title_.empty()) {
        return;
    }
    // iiSU jj2.c: a glass pill centred in the row, its text titleMedium.
    const TitlePillMetrics pill = metrics().titlePill();
    const float dp = dp_;
    const TextStyle text{pill.fontSize * dp};
    const float textWidth = std::min(type().measure(title_, text), pill.maxTextWidth * dp);
    const float bodyWidth =
        std::max(textWidth + 2.0f * pill.paddingHorizontal * dp, pill.minWidth * dp);
    const Rect body{(static_cast<float>(width_) - bodyWidth) * 0.5f, top + pill.topPadding * dp,
                    bodyWidth, pill.height * dp};
    glass_.paint(body);
    type().drawCentred(title_, body.centreX() - textWidth * 0.5f, body.centreY(), text, hintInk);
}

void Hud::drawHints() const {
    // iiSU mw5.h (BottomStart) and mw5.k (BottomEnd): jj2.b glass panels of a32.b entries.
    const HintPanelMetrics panel = metrics().hintPanels();
    drawPromptPanel(panel, leftPrompts, false);
    drawPromptPanel(panel,
                    startMenu_ ? std::span<const Prompt>{rightPromptsWithMenu}
                               : std::span<const Prompt>{rightPrompts},
                    true);
}

void Hud::drawPromptPanel(const HintPanelMetrics& panel, std::span<const Prompt> prompts,
                          bool atEnd) const {
    const float dp = dp_;
    const TextStyle text{panel.labelSize * dp, panel.labelTracking};
    const float glyph = panel.glyphSize * dp;
    const float gap = panel.glyphGap * dp;
    float labels = 0.0f;
    for (const auto& [key, label] : prompts) {
        labels = std::max(labels, type().measure(label, text));
    }
    const float width = 2.0f * panel.paddingHorizontal * dp + glyph + gap + labels;
    const float height = panel.height(dp) * dp;
    const float bottom = (atEnd ? panel.rightBottom : panel.leftBottom) * dp;
    const float x =
        atEnd ? static_cast<float>(width_) - panel.rightInset * dp - width : panel.leftInset * dp;
    const Rect body{x, static_cast<float>(height_) - bottom - height, width, height};
    glass_.paint(body, panel.cornerRadius * dp);

    // jj2.b: a Column of a32.b rows, centred vertically, rows centred on their tallest child.
    const float row = std::max(glyph, std::ceil(panel.labelLineHeight * dp));
    const auto count = static_cast<float>(prompts.size());
    const float content = count * row + (count - 1.0f) * panel.entrySpacing * dp;
    float top = body.centreY() - content * 0.5f;
    const float left = body.x + panel.paddingHorizontal * dp;
    for (const auto& [key, label] : prompts) {
        const float centreY = top + row * 0.5f;
        glyphs_.paint(key, Vector2{left + glyph * 0.5f, centreY}, glyph, hintInk);
        type().drawCentred(label, left + glyph + gap, centreY + panel.labelShift(key) * dp, text,
                           hintInk);
        top += row + panel.entrySpacing * dp;
    }
}

void Hud::drawToast() const {
    if (toast_.empty()) {
        return;
    }
    const float u = unit();
    const TextStyle text{type().emForLineBox(u * 1.4f)};
    const float textWidth = type().measure(toast_, text);
    const float padding = u * 2.0f;
    const Rectangle box{
        (static_cast<float>(width_) - textWidth - 2.0f * padding) / 2.0f,
        static_cast<float>(height_) - u * 8.0f,
        textWidth + 2.0f * padding,
        type().lineBox(text) + padding,
    };
    DrawRectangleRounded(box, 0.5f, 12,
                         toastError_ ? Color{0xb3, 0x26, 0x1e, 240} : Color{0x2b, 0x27, 0x33, 240});
    type().drawCentred(toast_, box.x + padding / 2.0f,
                       box.y + padding / 3.0f + type().lineBox(text) * 0.5f, text, WHITE);
}

} // namespace iideck::ui

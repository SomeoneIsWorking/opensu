#include "hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include "clock_text.hpp"
#include "corner_hints.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// iiSU res/drawable/bell_icon.png ink, which iiSU's hint glyphs and labels share.
constexpr Color hintInk{0x4D, 0x46, 0x55, 255};

} // namespace

void Hud::setSize(const Size& size) noexcept {
    width_ = size.width;
    height_ = size.height;
    dp_ = size.dp;
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
    for (int row = 1; spacing * static_cast<float>(row) < static_cast<float>(height_); ++row) {
        for (int column = 1; spacing * static_cast<float>(column) < static_cast<float>(width_);
             ++column) {
            DrawCircleV({spacing * static_cast<float>(column), spacing * static_cast<float>(row)},
                        radius, dot);
        }
    }
}

TopBarLayout Hud::topBarLayout() const {
    const auto visible =
        static_cast<std::size_t>(std::ranges::count_if(launchers_, [](const LauncherBadge& badge) {
            return badge.state != ServiceState::Hidden;
        }));
    return layoutTopBar(metrics(),
                        TopBarFrame{static_cast<float>(width_), static_cast<float>(height_), dp_,
                                    ClockText::hasLetters(clock_), visible});
}

std::optional<library::Source> Hud::launcherAt(float x, float y) const {
    const std::optional<std::size_t> cell = topBarLayout().launcherAt(x, y);
    if (!cell) {
        return std::nullopt;
    }
    std::size_t seen = 0;
    for (const LauncherBadge& badge : launchers_) {
        if (badge.state != ServiceState::Hidden && seen++ == *cell) {
            return badge.source;
        }
    }
    return std::nullopt;
}

void Hud::drawTopBar() {
    // iiSU mw5.l single-screen Row: friends | title (weight 1) | status, all Top. opensu has no
    // friends; its launchers' badges stand in the status pill where iiSU has the bell.
    const TopBarLayout layout = topBarLayout();
    const double seconds = std::chrono::duration<double>(now_.time_since_epoch()).count();
    statusPill_.paint(StatusPillView{layout.status, layout.pill, dp_, clock_, battery_,
                                     layout.launcherColumn, layout.divider, launchers_,
                                     BadgeRow{layout.launchers, launcherHover_, seconds}});
    // STOPGAP: jj2.w's glass strip behind the status pill is not drawn because its geometry is
    // not in the spec.

    drawTitlePill(layout.top);
    const BreadcrumbLayout trail = crumbs_.layout(trail_, crumbFrame(), metrics().titlePill());
    crumbs_.paint(trail_, trail, metrics().titlePill(), dp_, crumbHover_);
}

Rect Hud::titlePillBody(float top) const {
    const TitlePillMetrics pill = metrics().titlePill();
    const float dp = dp_;
    const TextStyle text{pill.fontSize * dp};
    const float textWidth = std::min(type().measure(title_, text), pill.maxTextWidth * dp);
    const float bodyWidth =
        std::max(textWidth + 2.0f * pill.paddingHorizontal * dp, pill.minWidth * dp);
    return Rect{(static_cast<float>(width_) - bodyWidth) * 0.5f, top + pill.topPadding * dp,
                bodyWidth, pill.height * dp};
}

BreadcrumbFrame Hud::crumbFrame() const {
    const TopBarLayout bar = topBarLayout();
    const TitlePillMetrics pill = metrics().titlePill();
    const float gap = TopBarMetrics::rowPaddingEnd * dp_;
    float right = bar.status.x - gap;
    if (!title_.empty()) {
        right = std::min(right, titlePillBody(bar.top).x - gap);
    }
    return BreadcrumbFrame{.left = TopBarMetrics::rowPaddingEnd * dp_,
                           .top = bar.top + pill.topPadding * dp_,
                           .height = pill.height * dp_,
                           .maxRight = right,
                           .dp = dp_};
}

std::optional<std::size_t> Hud::crumbAt(float x, float y) const {
    return crumbs_.layout(trail_, crumbFrame(), metrics().titlePill()).crumbAt(x, y);
}

void Hud::drawTitlePill(float top) const {
    if (title_.empty()) {
        return;
    }
    // iiSU jj2.c: a glass pill centred in the row, its text titleMedium.
    const TitlePillMetrics pill = metrics().titlePill();
    const TextStyle text{pill.fontSize * dp_};
    const float textWidth = std::min(type().measure(title_, text), pill.maxTextWidth * dp_);
    const Rect body = titlePillBody(top);
    glass_.paint(body);
    type().drawCentred(title_, body.centreX() - textWidth * 0.5f, body.centreY(), text, hintInk);
}

void Hud::drawHints() const {
    // iiSU mw5.h (BottomStart) and mw5.k (BottomEnd): jj2.b glass panels of a32.b entries.
    const HintPanelMetrics panel = metrics().hintPanels();
    drawPromptPanel(panel, startPrompts(hints_), false);
    drawPromptPanel(panel, endPrompts(hints_), true);
}

void Hud::drawPromptPanel(const HintPanelMetrics& panel, const std::vector<Prompt>& prompts,
                          bool atEnd) const {
    if (prompts.empty()) {
        return;
    }
    const float dp = dp_;
    const TextStyle text{panel.labelSize * dp, panel.labelTracking};
    const float glyph = panel.glyphSize * dp;
    const float gap = panel.glyphGap * dp;
    float labels = 0.0f;
    float column = glyph;
    for (const auto& [key, label] : prompts) {
        labels = std::max(labels, type().measure(label, text));
        column = std::max(column, glyphs_.advance(key, glyph));
    }
    const float width = 2.0f * panel.paddingHorizontal * dp + column + gap + labels;
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
        glyphs_.paint(key, Vector2{left + column * 0.5f, centreY}, glyph, hintInk);
        type().drawCentred(label, left + column + gap, centreY + panel.labelShift(key) * dp, text,
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

} // namespace opensu::ui

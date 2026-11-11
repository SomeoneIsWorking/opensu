#include "hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "clock_text.hpp"
#include "corner_hints.hpp"
#include "dock_metrics.hpp"
#include "gpu2d/ui/text_fit.hpp"
#include "interface_theme.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

// The clock bar's time and date either side of the dock, and their gap from it, in dp.
constexpr float clockTimeDp = 24.0f;
constexpr float clockDateDp = 14.0f;
constexpr float clockDockGapDp = 22.0f;
// The narrowest screen whose dock leaves the time and date room beside it, in dp.
constexpr float clockBesideDockDp = 600.0f;
// On a narrower one, their line above the prompt row, its sizes and the gap between the two.
constexpr float clockRowDp = 26.0f;
constexpr float clockRowTimeDp = 16.0f;
constexpr float clockRowDateDp = 12.5f;
constexpr float clockRowGapDp = 8.0f;

} // namespace

bool chromeDark(const theme::Palette& palette) noexcept {
    // iiSU gh3.q: the grid chrome is dark when the theme background's luminance is under half.
    return luminance(colourOf(palette.groundTop)) < 0.5f;
}

void Hud::setSize(const Size& size) noexcept {
    width_ = size.width;
    height_ = size.height;
    dp_ = size.dp;
    displayDp_ = size.displayDp;
    safe_ = size.safe;
}

float Hud::unit() const noexcept {
    return static_cast<float>(std::min(width_, height_)) / 100.0f;
}

TopBarMetrics Hud::metrics() const noexcept {
    return TopBarMetrics{static_cast<float>(width_) / dp_, static_cast<float>(height_) / dp_};
}

float Hud::topInset() const noexcept {
    return barHeight() + safe_.top;
}

float Hud::barHeight() const noexcept {
    return metrics().gridTopInset() * dp_;
}

float Hud::bottomInset() const noexcept {
    return metrics().gridBottomInset() * dp_ + clockRow() + safe_.bottom;
}

bool Hud::clockBesideDock() const noexcept {
    return static_cast<float>(width_) >= clockBesideDockDp * dp_;
}

float Hud::clockRow() const noexcept {
    return tokens_.home.clockBar && !clockBesideDock() ? clockRowDp * dp_ : 0.0f;
}

const std::string& Hud::pillClock() const noexcept {
    return tokens_.home.pillClock ? clock_ : noClock_;
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

void Hud::drawGround(Canvas& canvas) const {
    const theme::Palette& colours = tokens_.palette;
    const auto height = static_cast<float>(height_);
    const Rect whole{0.0f, 0.0f, static_cast<float>(width_), height};
    if (colours.groundTop == colours.groundBottom) {
        canvas.fillRect(onCanvas(whole), colourOf(colours.groundTop));
    } else {
        const Colour top = colourOf(colours.groundTop);
        const Colour bottom = colourOf(colours.groundBottom);
        fillRoundRect(canvas, RoundRect{whole, 0.0f}, [top, bottom, height](Point point, float) {
            return mix(top, bottom, point.y / std::max(height, 1.0f));
        });
    }
    if (!colours.groundDots) {
        return;
    }
    const float u = unit();
    const float spacing = std::max(u * 2.2f, 8.0f);
    const float radius = std::max(spacing * 0.09f, 1.0f);
    const Colour ink = colourOf(colours.ink);
    const Colour dot{ink.r, ink.g, ink.b, 24};
    for (int row = 1; spacing * static_cast<float>(row) < static_cast<float>(height_); ++row) {
        for (int column = 1; spacing * static_cast<float>(column) < static_cast<float>(width_);
             ++column) {
            canvas.fillCircle(
                Point{spacing * static_cast<float>(column), spacing * static_cast<float>(row)},
                radius, dot);
        }
    }
}

void Hud::drawClockBar(Canvas& canvas) const {
    if (!tokens_.home.clockBar) {
        return;
    }
    const theme::Palette& colours = tokens_.palette;
    const auto width = static_cast<float>(width_);
    const auto height = static_cast<float>(height_);
    const float top = height - bottomInset();
    const Rect band{0.0f, top, width, height - top};
    // A soft shadow over the ground above the band, then the band washed downwards.
    const float shade = 6.0f * dp_;
    const Colour dark = withAlpha(colourOf(colours.ink), 0.0f);
    const Colour edge = withAlpha(colourOf(colours.ink), 0.08f);
    fillRoundRect(canvas, RoundRect{Rect{0.0f, top - shade, width, shade}, 0.0f},
                  [dark, edge, top, shade](Point point, float) {
                      return mix(dark, edge,
                                 std::clamp(1.0f - (top - point.y) / shade, 0.0f, 1.0f));
                  });
    const Colour upper = colourOf(colours.surface);
    const Colour lower = colourOf(colours.ground);
    fillRoundRect(canvas, RoundRect{band, 0.0f}, [upper, lower, band](Point point, float) {
        return mix(upper, lower, (point.y - band.y) / std::max(band.height, 1.0f));
    });
    // A bright rim along its top.
    canvas.fillRect(onCanvas(Rect{0.0f, top, width, std::max(1.5f * dp_, 1.0f)}), white);
    const Colour ink = colourOf(colours.inkMuted);
    const Colour faint = colourOf(colours.inkFaint);
    if (clockBesideDock()) {
        const DockLayout dock = layoutDock(
            DockMetrics{width / dp_, height / dp_, {sectionItems.begin(), sectionItems.end()}},
            width, height, dp_);
        const float gap = clockDockGapDp * dp_;
        const float timeSize = clockTimeDp * dp_;
        const float centre = dock.bar.centreY();
        text_.drawCentred(
            canvas, clock_,
            Point{dock.leftBadge.x - gap - text_.measure(clock_, timeSize), centre}, timeSize, ink);
        text_.drawCentred(canvas, date_, Point{dock.rightBadge.right() + gap, centre},
                          clockDateDp * dp_, ink);
        return;
    }
    const float timeSize = clockRowTimeDp * dp_;
    const float dateSize = clockRowDateDp * dp_;
    const float timeWidth = text_.measure(clock_, timeSize);
    const float between = clockRowGapDp * dp_;
    const float left = (width - timeWidth - between - text_.measure(date_, dateSize)) * 0.5f;
    const float centre = top + clockRow() * 0.6f;
    text_.drawCentred(canvas, clock_, Point{left, centre}, timeSize, ink);
    text_.drawCentred(canvas, date_, Point{left + timeWidth + between, centre}, dateSize, faint);
}

TopBarLayout Hud::topBarLayout() const {
    const auto visible =
        static_cast<std::size_t>(std::ranges::count_if(launchers_, [](const LauncherBadge& badge) {
            return badge.state != ServiceState::Hidden;
        }));
    const bool letters = ClockText::hasLetters(pillClock());
    const float content =
        statusPill_.rowWidth(metrics().statusPill(letters), dp_, pillClock(), battery_);
    return layoutTopBar(metrics(),
                        TopBarFrame{static_cast<float>(width_), static_cast<float>(height_), dp_,
                                    letters, content, visible, safe_});
}

std::vector<Hud::BadgeCell> Hud::badgeCells() const {
    const TopBarLayout layout = topBarLayout();
    std::vector<BadgeCell> cells;
    std::size_t seen = 0;
    for (const LauncherBadge& badge : launchers_) {
        if (badge.state == ServiceState::Hidden || seen >= layout.launchers.size()) {
            continue;
        }
        const Rect& cell = layout.launchers[seen++];
        cells.push_back(BadgeCell{badge, Rect{cell.x - layout.hitSlop, cell.y - layout.hitSlop,
                                              cell.width + 2.0f * layout.hitSlop,
                                              cell.height + 2.0f * layout.hitSlop}});
    }
    return cells;
}

void Hud::drawTopBar(Canvas& canvas, const TopBarHover& hover) {
    // iiSU mw5.l single-screen Row: friends | title (weight 1) | status. opensu has no friends;
    // its launchers' badges stand in the status pill where iiSU has the bell.
    const TopBarLayout layout = topBarLayout();
    const double seconds = std::chrono::duration<double>(now_.time_since_epoch()).count();
    statusPill_.paint(canvas, StatusPillView{layout.status, layout.pill, dp_, pillClock(), battery_,
                                             layout.launcherColumn, layout.divider, launchers_,
                                             BadgeRow{layout.launchers, hover.launcher, seconds}});
    // STOPGAP: jj2.w's glass strip behind the status pill is not drawn because its geometry is
    // not in the spec.

    drawTitlePill(canvas, layout);
    const BreadcrumbLayout trail = crumbs_.layout(trail_, crumbFrame(), metrics().titlePill());
    crumbs_.paint(canvas, trail_, trail, metrics().titlePill(), dp_, hover.crumb);
    for (const TouchButton button : {TouchButton::Back, TouchButton::Menu}) {
        if (const std::optional<TouchButtonCell> cell = touchButtonCell(button)) {
            touchButtonPainter_.paint(canvas, button, cell->body, dp_, hover.button == button);
        }
    }
}

std::optional<Hud::TouchButtonCell> Hud::touchButtonCell(TouchButton button) const {
    if (!(button == TouchButton::Back ? touchButtons_.back : touchButtons_.menu)) {
        return std::nullopt;
    }
    const TopBarLayout bar = topBarLayout();
    const float size = touchButtonDp * fingerDp(Density{.dp = dp_, .display = displayDp_});
    const float gap = TopBarMetrics::rowPaddingEnd * dp_;
    // At the trail's start or just before the status pill.
    const float x = button == TouchButton::Back ? safe_.left + gap : bar.status.x - gap - size;
    const Rect hit{x, bar.centreY() - size * 0.5f, size, size};
    return TouchButtonCell{Rect{hit.centreX() - bar.height * 0.5f, bar.top, bar.height, bar.height},
                           hit};
}

std::optional<Hud::TitleCell> Hud::titleCell(const TopBarLayout& bar) const {
    if (title_.empty()) {
        return std::nullopt;
    }
    const TitlePillMetrics pill = metrics().titlePill();
    const float dp = dp_;
    const float padding = 2.0f * pill.paddingHorizontal * dp;
    const float wanted =
        std::min(text_.measure(title_, pill.fontSize * dp), pill.maxTextWidth * dp);
    const BreadcrumbFrame room = trailRoom(bar);
    BreadcrumbFrame shortest = room;
    shortest.maxRight = room.left;
    const float gap = TopBarMetrics::rowPaddingEnd * dp;
    // The trail gives way to its shortest labels before the title does.
    const float trailEnd = crumbs_.layout(trail_, room, pill).bar.right();
    const float trailFloor = crumbs_.layout(trail_, shortest, pill).bar.right();
    const float bodyWidth =
        std::min(std::max(wanted + padding, pill.minWidth * dp), room.maxRight - trailFloor - gap);
    if (bodyWidth < pill.minWidth * dp) {
        return std::nullopt;
    }
    // Centred, unless the trail reaches past that on a narrow screen: then just after it.
    const float centred = (static_cast<float>(width_) - bodyWidth) * 0.5f;
    const float x = std::min(std::max(centred, trailEnd + gap), room.maxRight - bodyWidth);
    return TitleCell{.body = Rect{x, bar.top, bodyWidth, bar.height},
                     .textWidth = std::min(wanted, bodyWidth - padding)};
}

BreadcrumbFrame Hud::crumbFrame() const {
    const TopBarLayout bar = topBarLayout();
    BreadcrumbFrame frame = trailRoom(bar);
    if (const std::optional<TitleCell> title = titleCell(bar)) {
        frame.maxRight =
            std::min(frame.maxRight, title->body.x - TopBarMetrics::rowPaddingEnd * dp_);
    }
    return frame;
}

BreadcrumbFrame Hud::trailRoom(const TopBarLayout& bar) const {
    const float gap = TopBarMetrics::rowPaddingEnd * dp_;
    float left = safe_.left + gap;
    float right = bar.status.x - gap;
    // The touch buttons keep their room; the trail stands between them.
    if (const std::optional<TouchButtonCell> back = touchButtonCell(TouchButton::Back)) {
        left = back->hit.right() + gap;
    }
    if (const std::optional<TouchButtonCell> menu = touchButtonCell(TouchButton::Menu)) {
        right = std::min(right, menu->hit.x - gap);
    }
    return BreadcrumbFrame{
        .left = left, .top = bar.top, .height = bar.height, .maxRight = right, .dp = dp_};
}

std::vector<Rect> Hud::crumbCells() const {
    return crumbs_.layout(trail_, crumbFrame(), metrics().titlePill()).cells;
}

void Hud::drawTitlePill(Canvas& canvas, const TopBarLayout& bar) const {
    const std::optional<TitleCell> title = titleCell(bar);
    if (!title) {
        return;
    }
    // iiSU jj2.c: a glass pill centred in the row, its text titleMedium.
    const float size = metrics().titlePill().fontSize * dp_;
    const std::string text = gpu2d::ui::fitted(text_, title_, title->textWidth, size);
    const float textWidth = std::min(text_.measure(text, size), title->textWidth);
    glass_.paint(canvas, title->body);
    text_.drawCentred(canvas, text,
                      Point{title->body.centreX() - textWidth * 0.5f, title->body.centreY()}, size,
                      colourOf(tokens_.palette.chromeInk));
}

void Hud::drawHints(Canvas& canvas) const {
    // iiSU mw5.h (BottomStart) and mw5.k (BottomEnd): jj2.b glass panels of a32.b entries.
    drawPromptPanel(canvas, startPrompts(hints_), false);
    drawPromptPanel(canvas, endPrompts(hints_), true);
}

std::optional<Hud::PromptPanel> Hud::promptPanel(const std::vector<Prompt>& prompts,
                                                 bool atEnd) const {
    if (prompts.empty()) {
        return std::nullopt;
    }
    const HintPanelMetrics panel = metrics().hintPanels();
    const float dp = dp_;
    const float text = panel.labelSize * dp;
    const float glyph = panel.glyphSize * dp;
    const float gap = panel.glyphGap * dp;
    float labels = 0.0f;
    PromptPanel placed;
    placed.column = glyph;
    for (const Prompt& prompt : prompts) {
        labels = std::max(labels, text_.measure(prompt.label, text));
        placed.column = std::max(placed.column, glyphs_.advance(prompt.key, glyph));
    }
    const float padding = panel.paddingHorizontal * dp;
    const float bottom = (atEnd ? panel.rightBottom : panel.leftBottom) * dp;
    const float floor = static_cast<float>(height_) - safe_.bottom - bottom;
    if (touchButtons_.prompts) {
        // Side by side, each a finger's height, so the panel grows across rather than up the
        // screen; iiSU stacks them for a pad.
        const gpu2d::ui::Theme theme =
            interfaceTheme(Density{.dp = dp_, .display = displayDp_}, tokens_);
        const float tall = std::max(panel.height(dp) * dp, theme.px(theme.spacing.touchTarget));
        float x = 0.0f;
        for (const Prompt& prompt : prompts) {
            const float wide =
                2.0f * padding + placed.column + gap + text_.measure(prompt.label, text);
            placed.rows.emplace_back(prompt, Rect{x, floor - tall, wide, tall});
            x += wide;
        }
        const float start =
            atEnd ? static_cast<float>(width_) - safe_.right - panel.rightInset * dp - x
                  : safe_.left + panel.leftInset * dp;
        for (auto& [prompt, row] : placed.rows) {
            row.x += start;
        }
        placed.body = Rect{start, floor - tall, x, tall};
        return placed;
    }
    const float width = 2.0f * padding + placed.column + gap + labels;
    const float height = panel.height(dp) * dp;
    const float x = atEnd ? static_cast<float>(width_) - safe_.right - panel.rightInset * dp - width
                          : safe_.left + panel.leftInset * dp;
    placed.body = Rect{x, floor - height, width, height};

    // jj2.b: a Column of a32.b rows, centred vertically, rows centred on their tallest child.
    const float row = std::max(glyph, std::ceil(panel.labelLineHeight * dp));
    const auto count = static_cast<float>(prompts.size());
    const float content = count * row + (count - 1.0f) * panel.entrySpacing * dp;
    float top = placed.body.centreY() - content * 0.5f;
    for (const Prompt& prompt : prompts) {
        placed.rows.emplace_back(prompt, Rect{placed.body.x, top, placed.body.width, row});
        top += row + panel.entrySpacing * dp;
    }
    return placed;
}

void Hud::drawPromptPanel(Canvas& canvas, const std::vector<Prompt>& prompts, bool atEnd) const {
    const std::optional<PromptPanel> placed = promptPanel(prompts, atEnd);
    if (!placed) {
        return;
    }
    const HintPanelMetrics panel = metrics().hintPanels();
    const float dp = dp_;
    const float text = panel.labelSize * dp;
    const float glyph = panel.glyphSize * dp;
    const float gap = panel.glyphGap * dp;
    glass_.paint(canvas, placed->body, panel.cornerRadius * dp);
    for (const auto& [prompt, row] : placed->rows) {
        const float left = row.x + panel.paddingHorizontal * dp;
        const float centreY = row.centreY();
        glyphs_.paint(canvas, prompt.key, Point{left + placed->column * 0.5f, centreY}, glyph,
                      colourOf(tokens_.palette.chromeInk));
        text_.drawCentred(
            canvas, prompt.label,
            Point{left + placed->column + gap, centreY + panel.labelShift(prompt.key) * dp}, text,
            colourOf(tokens_.palette.chromeInk));
    }
}

std::vector<Hud::PromptCell> Hud::promptCells() const {
    std::vector<PromptCell> cells;
    const gpu2d::ui::Theme theme =
        interfaceTheme(Density{.dp = dp_, .display = displayDp_}, tokens_);
    const float least = theme.px(theme.spacing.touchTarget);
    for (const bool atEnd : {false, true}) {
        const std::optional<PromptPanel> placed =
            promptPanel(atEnd ? endPrompts(hints_) : startPrompts(hints_), atEnd);
        if (!placed) {
            continue;
        }
        for (const auto& [prompt, row] : placed->rows) {
            const float width = std::max(row.width, least);
            const float x = atEnd ? row.right() - width : row.x;
            cells.push_back(PromptCell{prompt, Rect{x, row.y, width, row.height}});
        }
    }
    return cells;
}

void Hud::drawToast(Canvas& canvas, float floor) const {
    if (toast_.empty()) {
        return;
    }
    // In dp like the rest of the interface, so it reads the same on a dense phone screen.
    const gpu2d::ui::Theme theme =
        interfaceTheme(Density{.dp = dp_, .display = displayDp_}, tokens_);
    const float u = unit();
    const float size = theme.px(theme.type.toast);
    const float padding = theme.px(theme.spacing.pad);
    // A padding of screen on each side of the box.
    const float room = static_cast<float>(width_) - 4.0f * padding;
    const std::vector<std::string> lines = gpu2d::ui::wrapped(text_, toast_, room, size);
    float textWidth = 0.0f;
    for (const std::string& line : lines) {
        textWidth = std::max(textWidth, text_.measure(line, size));
    }
    const float lineHeight = text_.lineHeight(size);
    const float boxHeight = lineHeight * static_cast<float>(lines.size()) + padding;
    const Rect box{
        (static_cast<float>(width_) - textWidth - 2.0f * padding) / 2.0f,
        std::min(static_cast<float>(height_) - u * 8.0f, floor - u - boxHeight),
        textWidth + 2.0f * padding,
        boxHeight,
    };
    // A toast of several lines keeps the corners of one.
    const Rect oneLine{box.x, box.y, box.width, lineHeight + padding};
    canvas.fillRoundRect(onCanvas(box), cornerRadius(oneLine, 0.5f),
                         toastError_ ? theme.colours.toastError : theme.colours.toast);
    float centreY = box.y + (padding + lineHeight) / 2.0f;
    for (const std::string& line : lines) {
        const float x = (static_cast<float>(width_) - text_.measure(line, size)) / 2.0f;
        text_.drawCentred(canvas, line, Point{x, centreY}, size, theme.colours.onToast);
        centreY += lineHeight;
    }
}

} // namespace opensu::ui

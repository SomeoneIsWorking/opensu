#include "launch_panel_painter.hpp"

#include <algorithm>
#include <cmath>

#include "hud.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color track{palette::inkSoft.r, palette::inkSoft.g, palette::inkSoft.b, 50};
constexpr Color hintInk{0x4d, 0x46, 0x55, 255};

constexpr float cardRadiusDp = 24.0f;
constexpr float titleSp = 24.0f;
constexpr float lineSp = 16.0f;
constexpr float dotSpacingDp = 18.0f;
constexpr int dotCount = 3;
constexpr double dotPeriodSeconds = 1.2;
constexpr float hintSp = 14.0f;
constexpr float hintGapDp = 6.0f;

/// Draws `text` centred horizontally on `centreX`.
void drawMiddle(std::string_view text, float centreX, float centreY, const TextStyle& style,
                Color colour) {
    type().drawCentred(text, centreX - type().measure(text, style) * 0.5f, centreY, style, colour);
}

} // namespace

PanelLayout LaunchPanelPainter::layout(const LaunchPanel& panel, Vector2 size, float dp) const {
    PanelMetrics metrics{.titleBox = type().lineBox(TextStyle{titleSp * dp}),
                         .lineBox = type().lineBox(TextStyle{lineSp * dp}),
                         .hintWidths = {}};
    const TextStyle hint{hintSp * dp};
    const float glyph = panelGlyphDp * dp;
    for (const PanelHint& entry : panel.hints()) {
        metrics.hintWidths.push_back(glyphs_.advance(entry.button, glyph) + hintGapDp * dp +
                                     type().measure(entry.action, hint));
    }
    return layoutPanel(size.x, size.y, dp, metrics);
}

void LaunchPanelPainter::paint(const LaunchPanel& panel, Vector2 size, float dp,
                               double seconds) const {
    if (!panel.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, scrim);

    const PanelLayout frame = layout(panel, size, dp);
    const Rectangle card{frame.card.x, frame.card.y, frame.card.width, frame.card.height};
    DrawRectangleRounded(card, std::min(1.0f, 2.0f * cardRadiusDp * dp / card.height), 16,
                         palette::panel);

    drawMiddle(panel.title(), frame.centreX, frame.titleY, TextStyle{titleSp * dp}, palette::ink);
    drawMiddle(panel.line(), frame.centreX, frame.lineY, TextStyle{lineSp * dp}, palette::inkSoft);

    const float y = frame.meterY;
    if (const std::optional<double> fraction = panel.fraction()) {
        const float barWidth = card.width - 2.0f * frame.padding;
        const float barHeight = frame.meter;
        const Rectangle bar{card.x + frame.padding, y - barHeight * 0.5f, barWidth, barHeight};
        DrawRectangleRounded(bar, 1.0f, 8, track);
        const Rectangle done{
            bar.x, bar.y, std::max(barHeight, barWidth * static_cast<float>(*fraction)), barHeight};
        DrawRectangleRounded(done, 1.0f, 8, palette::ink);
    } else if (panel.busy()) {
        // One dot lit at a time, left to right: the launch is alive but has no measure.
        const int lit =
            static_cast<int>(std::fmod(seconds, dotPeriodSeconds) / dotPeriodSeconds * dotCount);
        const float spacing = dotSpacingDp * dp;
        for (int i = 0; i < dotCount; ++i) {
            const float x =
                frame.centreX + (static_cast<float>(i) - (dotCount - 1) * 0.5f) * spacing;
            DrawCircleV(Vector2{x, y}, frame.meter * 0.5f,
                        i == lit ? static_cast<Color>(palette::ink) : track);
        }
    }

    const TextStyle hint{hintSp * dp};
    for (std::size_t i = 0; i < panel.hints().size(); ++i) {
        const PanelHint& entry = panel.hints()[i];
        const float advance = glyphs_.advance(entry.button, frame.glyph);
        const float x = frame.hints[i].x;
        glyphs_.paint(entry.button, Vector2{x + advance * 0.5f, frame.hintY}, frame.glyph, hintInk);
        type().drawCentred(entry.action, x + advance + hintGapDp * dp, frame.hintY, hint, hintInk);
    }
}

} // namespace opensu::ui

#include "launch_panel_painter.hpp"

#include <algorithm>
#include <cmath>

#include "hud.hpp"
#include "paint.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour track{palette::inkSoft.r, palette::inkSoft.g, palette::inkSoft.b, 50};
constexpr Colour hintInk{0x4d, 0x46, 0x55, 255};

constexpr float cardRadiusDp = 24.0f;
constexpr float titleSp = 24.0f;
constexpr float lineSp = 16.0f;
constexpr float dotSpacingDp = 18.0f;
constexpr int dotCount = 3;
constexpr double dotPeriodSeconds = 1.2;
constexpr float hintSp = 14.0f;
constexpr float hintGapDp = 6.0f;

/// Draws `text` centred horizontally on `centreX`.
void drawMiddle(Canvas& canvas, Typeface& typeface, std::string_view text, float centreX,
                float centreY, const TextStyle& style, Colour colour) {
    typeface.drawCentred(canvas, text, centreX - typeface.measure(text, style) * 0.5f, centreY,
                         style, colour);
}

} // namespace

PanelLayout LaunchPanelPainter::layout(const LaunchPanel& panel, Point size, float dp) const {
    PanelMetrics metrics{.titleBox = typeface_.lineBox(TextStyle{titleSp * dp}),
                         .lineBox = typeface_.lineBox(TextStyle{lineSp * dp}),
                         .hintWidths = {}};
    const TextStyle hint{hintSp * dp};
    const float glyph = panelGlyphDp * dp;
    for (const PanelHint& entry : panel.hints()) {
        metrics.hintWidths.push_back(glyphs_.advance(entry.button, glyph) + hintGapDp * dp +
                                     typeface_.measure(entry.action, hint));
    }
    return layoutPanel(size.x, size.y, dp, metrics);
}

void LaunchPanelPainter::paint(Canvas& canvas, const LaunchPanel& panel, Point size, float dp,
                               double seconds) const {
    if (!panel.isOpen()) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, scrim);

    const PanelLayout frame = layout(panel, size, dp);
    const Rect& card = frame.card;
    canvas.fillRoundRect(onCanvas(card),
                         cornerRadius(card, std::min(1.0f, 2.0f * cardRadiusDp * dp / card.height)),
                         palette::panel);

    drawMiddle(canvas, typeface_, panel.title(), frame.centreX, frame.titleY,
               TextStyle{titleSp * dp}, palette::ink);
    drawMiddle(canvas, typeface_, panel.line(), frame.centreX, frame.lineY, TextStyle{lineSp * dp},
               palette::inkSoft);

    const float y = frame.meterY;
    if (const std::optional<double> fraction = panel.fraction()) {
        const float barWidth = card.width - 2.0f * frame.padding;
        const float barHeight = frame.meter;
        const Rect bar{card.x + frame.padding, y - barHeight * 0.5f, barWidth, barHeight};
        canvas.fillRoundRect(onCanvas(bar), cornerRadius(bar, 1.0f), track);
        const Rect done{bar.x, bar.y, std::max(barHeight, barWidth * static_cast<float>(*fraction)),
                        barHeight};
        canvas.fillRoundRect(onCanvas(done), cornerRadius(done, 1.0f), palette::ink);
    } else if (panel.busy()) {
        // One dot lit at a time, left to right: the launch is alive but has no measure.
        const int lit =
            static_cast<int>(std::fmod(seconds, dotPeriodSeconds) / dotPeriodSeconds * dotCount);
        const float spacing = dotSpacingDp * dp;
        for (int i = 0; i < dotCount; ++i) {
            const float x =
                frame.centreX + (static_cast<float>(i) - (dotCount - 1) * 0.5f) * spacing;
            canvas.fillCircle(Point{x, y}, frame.meter * 0.5f, i == lit ? palette::ink : track);
        }
    }

    const TextStyle hint{hintSp * dp};
    for (std::size_t i = 0; i < panel.hints().size(); ++i) {
        const PanelHint& entry = panel.hints()[i];
        const float advance = glyphs_.advance(entry.button, frame.glyph);
        const float x = frame.hints[i].x;
        glyphs_.paint(canvas, entry.button, Point{x + advance * 0.5f, frame.hintY}, frame.glyph,
                      hintInk);
        typeface_.drawCentred(canvas, entry.action, x + advance + hintGapDp * dp, frame.hintY, hint,
                              hintInk);
    }
}

} // namespace opensu::ui

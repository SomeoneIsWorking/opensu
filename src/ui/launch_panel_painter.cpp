#include "launch_panel_painter.hpp"

#include <algorithm>
#include <cmath>

#include "hud.hpp"
#include "typeface.hpp"

namespace iideck::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color track{palette::inkSoft.r, palette::inkSoft.g, palette::inkSoft.b, 50};
constexpr Color hintInk{0x4d, 0x46, 0x55, 255};

constexpr float cardWidthDp = 420.0f;
constexpr float cardRadiusDp = 24.0f;
constexpr float paddingDp = 28.0f;
constexpr float titleSp = 24.0f;
constexpr float lineSp = 16.0f;
constexpr float gapDp = 14.0f;
constexpr float barHeightDp = 8.0f;
constexpr float dotDp = 8.0f;
constexpr float dotSpacingDp = 18.0f;
constexpr int dotCount = 3;
constexpr double dotPeriodSeconds = 1.2;
constexpr float hintSp = 14.0f;
constexpr float hintGlyphDp = 20.0f;
constexpr float hintGapDp = 6.0f;

/// Draws `text` centred horizontally on `centreX`.
void drawMiddle(std::string_view text, float centreX, float centreY, const TextStyle& style,
                Color colour) {
    type().drawCentred(text, centreX - type().measure(text, style) * 0.5f, centreY, style, colour);
}

} // namespace

void LaunchPanelPainter::paint(const LaunchPanel& panel, float width, float height, float dp,
                               double seconds) const {
    if (!panel.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, width, height}, scrim);

    const float pad = paddingDp * dp;
    const float gap = gapDp * dp;
    const TextStyle title{titleSp * dp};
    const TextStyle line{lineSp * dp};
    const TextStyle hint{hintSp * dp};
    const float meter = std::max(barHeightDp, dotDp) * dp;
    const float glyph = hintGlyphDp * dp;
    const float cardHeight = pad + type().lineBox(title) + gap + type().lineBox(line) + gap +
                             meter + gap * 1.5f + glyph + pad;
    const float cardWidth = std::min(cardWidthDp * dp, width - 2.0f * pad);
    const Rectangle card{(width - cardWidth) * 0.5f, (height - cardHeight) * 0.5f, cardWidth,
                         cardHeight};
    DrawRectangleRounded(card, std::min(1.0f, 2.0f * cardRadiusDp * dp / cardHeight), 16,
                         palette::panel);

    const float centreX = card.x + card.width * 0.5f;
    float y = card.y + pad + type().lineBox(title) * 0.5f;
    drawMiddle(panel.title(), centreX, y, title, palette::ink);
    y += type().lineBox(title) * 0.5f + gap + type().lineBox(line) * 0.5f;
    drawMiddle(panel.line(), centreX, y, line, palette::inkSoft);
    y += type().lineBox(line) * 0.5f + gap + meter * 0.5f;

    if (const std::optional<double> fraction = panel.fraction()) {
        const float barWidth = card.width - 2.0f * pad;
        const float barHeight = barHeightDp * dp;
        const Rectangle bar{card.x + pad, y - barHeight * 0.5f, barWidth, barHeight};
        DrawRectangleRounded(bar, 1.0f, 8, track);
        const Rectangle done{
            bar.x, bar.y, std::max(barHeight, barWidth * static_cast<float>(*fraction)), barHeight};
        DrawRectangleRounded(done, 1.0f, 8, palette::ink);
    } else {
        // One dot lit at a time, left to right: the launch is alive but has no measure.
        const int lit =
            static_cast<int>(std::fmod(seconds, dotPeriodSeconds) / dotPeriodSeconds * dotCount);
        const float spacing = dotSpacingDp * dp;
        for (int i = 0; i < dotCount; ++i) {
            const float x = centreX + (static_cast<float>(i) - (dotCount - 1) * 0.5f) * spacing;
            DrawCircleV(Vector2{x, y}, dotDp * dp * 0.5f,
                        i == lit ? static_cast<Color>(palette::ink) : track);
        }
    }
    y += meter * 0.5f + gap * 1.5f + glyph * 0.5f;

    const char* label = "Cancel";
    const float hintWidth = glyph + hintGapDp * dp + type().measure(label, hint);
    const float x = centreX - hintWidth * 0.5f;
    glyphs_.paint("B", Vector2{x + glyph * 0.5f, y}, glyph, hintInk);
    type().drawCentred(label, x + glyph + hintGapDp * dp, y, hint, hintInk);
}

} // namespace iideck::ui

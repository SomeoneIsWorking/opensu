#include "chord_hint_painter.hpp"

#include <algorithm>

#include "hud.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr float paddingDp = 18.0f;
constexpr float cornerDp = 20.0f;
constexpr float glyphDp = 24.0f;
constexpr float rowDp = 36.0f;
constexpr float glyphGapDp = 12.0f;
constexpr float ruleGapDp = 8.0f;
constexpr float ruleDp = 1.5f;
constexpr float titleSp = 15.0f;
constexpr float labelSp = 17.0f;
constexpr float minWidthDp = 240.0f;
constexpr std::string_view title = "Hold and press";

} // namespace

void ChordHintPainter::paint(Canvas& canvas, const ChordHint& hint, Point size, float dp,
                             const PanelLook& look) const {
    const float unit = dp * look.scale;
    const float glyph = glyphDp * unit;
    const float gap = glyphGapDp * unit;
    const float heading = titleSp * unit;
    const float label = labelSp * unit;

    float column = glyphs_.advance(hint.modifierGlyph(), glyph);
    for (const ChordHintRow& row : hint.rows()) {
        column = std::max(column, glyphs_.advance(row.glyph, glyph));
    }
    float text = text_.measure(title, heading);
    for (const ChordHintRow& row : hint.rows()) {
        text = std::max(text, text_.measure(row.label, label));
    }
    const float padding = paddingDp * unit;
    const float width = std::max(minWidthDp * unit, column + gap + text + 2.0f * padding);
    const float rows = static_cast<float>(hint.rows().size() + 1);
    const float height =
        rows * rowDp * unit + 2.0f * ruleGapDp * unit + 2.0f * padding - (rowDp - glyphDp) * unit;
    const Rect panel{(size.x - width) * 0.5f, (size.y - height) * 0.5f, width, height};

    // Solid, as the volume display, so a busy game behind never muddies the list.
    const RoundRect shape{panel, cornerDp * unit};
    fillRoundRect(canvas, shape, [alpha = look.alpha](Point, float) {
        return withAlpha(Colour{0xF7, 0xF6, 0xFA, 255}, alpha);
    });
    fillBand(canvas, shape, shape.grown(-1.5f * dp), [alpha = look.alpha](Point, float) {
        return withAlpha(palette::off, alpha);
    });

    const Colour ink = withAlpha(palette::control, look.alpha);
    const Colour soft = withAlpha(palette::inkSoft, look.alpha);
    const float glyphX = panel.x + padding + column * 0.5f;
    const float textX = panel.x + padding + column + gap;
    float centreY = panel.y + padding + glyph * 0.5f;
    glyphs_.paint(canvas, hint.modifierGlyph(), Point{glyphX, centreY}, glyph, ink);
    text_.drawCentred(canvas, title, Point{textX, centreY}, heading, soft);

    centreY += glyph * 0.5f + ruleGapDp * unit;
    canvas.fillRect(
        gpu2d::Rect{panel.x + padding, centreY, panel.width - 2.0f * padding, ruleDp * unit},
        withAlpha(palette::off, look.alpha));
    centreY += ruleGapDp * unit + rowDp * unit * 0.5f;
    for (const ChordHintRow& row : hint.rows()) {
        glyphs_.paint(canvas, row.glyph, Point{glyphX, centreY}, glyph, ink);
        text_.drawCentred(canvas, row.label, Point{textX, centreY}, label, ink);
        centreY += rowDp * unit;
    }
}

} // namespace opensu::ui

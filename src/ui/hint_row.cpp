#include "hint_row.hpp"

namespace opensu::ui {
namespace {

constexpr float hintSp = 13.0f;
constexpr float hintGlyphDp = 18.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 18.0f;

} // namespace

float hintRowWidth(const ButtonGlyphPainter& glyphs, Typeface& typeface,
                   std::span<const HintPrompt> hints, float dp) {
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    float width = 0.0f;
    for (const auto& [key, label] : hints) {
        width += glyphs.advance(key, glyph) + hintGapDp * dp + typeface.measure(label, hint) +
                 hintSpacingDp * dp;
    }
    return hints.empty() ? 0.0f : width - hintSpacingDp * dp;
}

void paintHintRow(Canvas& canvas, const ButtonGlyphPainter& glyphs, Typeface& typeface,
                  std::span<const HintPrompt> hints, Point centre, float dp, Colour ink) {
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    float x = centre.x - hintRowWidth(glyphs, typeface, hints, dp) * 0.5f;
    for (const auto& [key, label] : hints) {
        const float advance = glyphs.advance(key, glyph);
        glyphs.paint(canvas, key, Point{x + advance * 0.5f, centre.y}, glyph, ink);
        x += advance + hintGapDp * dp;
        typeface.drawCentred(canvas, label, x, centre.y, hint, ink);
        x += typeface.measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui

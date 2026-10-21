#include "side_panel_painter.hpp"

#include "hud.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour panelFill = palette::ground;
constexpr Colour hintInk{0x4d, 0x46, 0x55, 255};

constexpr float headingSp = 22.0f;
constexpr float hintSp = 14.0f;
constexpr float hintGlyphDp = 20.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 20.0f;

} // namespace

float SidePanelPainter::headingBox(float dp) const {
    return typeface_.lineBox(TextStyle{headingSp * dp});
}

float SidePanelPainter::footer(float dp) noexcept {
    return hintGlyphDp * dp;
}

void SidePanelPainter::paintGround(Canvas& canvas, const Rect& panel, float width,
                                   float height) const {
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, width, height}, scrim);
    canvas.fillRect(gpu2d::Rect{panel.x, 0.0f, panel.width, height}, panelFill);
}

void SidePanelPainter::paintHeading(Canvas& canvas, const std::string& text, const Rect& heading,
                                    float dp) const {
    const TextStyle style{headingSp * dp};
    typeface_.drawCentred(canvas, typeface_.fitted(text, heading.width, style), heading.x,
                          heading.centreY(), style, palette::ink);
}

void SidePanelPainter::paintHints(Canvas& canvas, const std::vector<PanelHint>& hints, float x,
                                  float height, float padding, float dp) const {
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float centreY = height - padding - glyph * 0.5f;
    for (const PanelHint& each : hints) {
        const float advance = glyphs_.advance(each.button, glyph);
        glyphs_.paint(canvas, each.button, Point{x + advance * 0.5f, centreY}, glyph, hintInk);
        x += advance + hintGapDp * dp;
        typeface_.drawCentred(canvas, each.action, x, centreY, hint, hintInk);
        x += typeface_.measure(each.action, hint) + hintSpacingDp * dp;
    }
}

void SidePanelPainter::paintGlyph(Canvas& canvas, const std::string& key, Point centre, float box,
                                  Colour ink) const {
    glyphs_.paint(canvas, key, centre, box, ink);
}

} // namespace opensu::ui

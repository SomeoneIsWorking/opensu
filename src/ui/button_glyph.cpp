#include "button_glyph.hpp"

#include "typeface.hpp"

namespace iideck::ui {
namespace {

// input_glyph_minus.xml and input_glyph_plus.xml, in their 24-unit viewport: a radius 10 ring
// stroked 1.5, and bars from 7.9 to 16.1 stroked 2 with round caps.
constexpr float vectorViewport = 24.0f;
constexpr float vectorRingRadius = 10.0f;
constexpr float vectorRingStroke = 1.5f;
constexpr float vectorBarHalf = 4.1f;
constexpr float vectorBarStroke = 2.0f;
// input_glyph_{a,b,x,y}.png, 96 px, measured at half alpha.
constexpr float pngRingOuter = 40.0f / 96.0f;
constexpr float pngRingStroke = 6.0f / 96.0f;
constexpr float pngCapHeight = 42.0f / 96.0f;
// Cal Sans capitals are 0.7 em (OS/2 sCapHeight 700 of 1000).
constexpr float capHeightPerEm = 0.7f;

void roundBar(Vector2 from, Vector2 to, float stroke, Color ink) {
    DrawLineEx(from, to, stroke, ink);
    DrawCircleV(from, stroke * 0.5f, ink);
    DrawCircleV(to, stroke * 0.5f, ink);
}

} // namespace

void ButtonGlyphPainter::paint(std::string_view key, Vector2 centre, float box, Color ink) const {
    if (key == "-" || key == "+") {
        const float unit = box / vectorViewport;
        const float ring = vectorRingRadius * unit;
        const float half = vectorRingStroke * unit * 0.5f;
        DrawRing(centre, ring - half, ring + half, 0.0f, 360.0f, 48, ink);
        const float bar = vectorBarHalf * unit;
        const float stroke = vectorBarStroke * unit;
        roundBar({centre.x - bar, centre.y}, {centre.x + bar, centre.y}, stroke, ink);
        if (key == "+") {
            roundBar({centre.x, centre.y - bar}, {centre.x, centre.y + bar}, stroke, ink);
        }
        return;
    }
    const float outer = pngRingOuter * box;
    DrawRing(centre, outer - pngRingStroke * box, outer, 0.0f, 360.0f, 48, ink);
    const TextStyle letter{pngCapHeight * box / capHeightPerEm};
    const float width = type().measure(key, letter);
    type().drawCentred(key, centre.x - width * 0.5f, centre.y, letter, ink);
}

} // namespace iideck::ui

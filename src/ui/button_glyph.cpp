#include "button_glyph.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "input/keyboard_bindings.hpp"
#include "typeface.hpp"

namespace opensu::ui {
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

// lb_button.png and rb_button.png, 249 px: the outline spans 216 px, its large corner 85 px and
// the others 25 px, and the letters stand 56 px tall.
constexpr float shoulderSide = 216.0f / 249.0f;
constexpr float shoulderBigCorner = 85.0f / 216.0f;
constexpr float shoulderSmallCorner = 25.0f / 216.0f;
constexpr float shoulderStroke = 14.0f / 249.0f;
constexpr float shoulderCapHeight = 56.0f / 249.0f;
constexpr int cornerSegments = 12;

// A key cap is the ring's height, with the ring's stroke and cap height, and grows to fit a long
// label.
constexpr float capRoundness = 0.3f;
constexpr float capLabelPadding = 0.18f;
constexpr int capSegments = 12;

void roundBar(Vector2 from, Vector2 to, float stroke, Color ink) {
    DrawLineEx(from, to, stroke, ink);
    DrawCircleV(from, stroke * 0.5f, ink);
    DrawCircleV(to, stroke * 0.5f, ink);
}

/// The outline of a square of `side` about `centre` whose top corner on the `bigSide` has the large
/// radius, as points clockwise from the top-left.
std::vector<Vector2> shoulderOutline(Vector2 centre, float side, bool bigOnLeft) {
    const float half = side * 0.5f;
    const float big = shoulderBigCorner * side;
    const float small = shoulderSmallCorner * side;
    // Radius per corner, clockwise from the top-left, and each corner's arc centre.
    const std::array<float, 4> radius{bigOnLeft ? big : small, bigOnLeft ? small : big, small,
                                      small};
    const std::array<Vector2, 4> corner{
        Vector2{centre.x - half, centre.y - half}, Vector2{centre.x + half, centre.y - half},
        Vector2{centre.x + half, centre.y + half}, Vector2{centre.x - half, centre.y + half}};
    const std::array<Vector2, 4> inward{Vector2{1.0f, 1.0f}, Vector2{-1.0f, 1.0f},
                                        Vector2{-1.0f, -1.0f}, Vector2{1.0f, -1.0f}};
    std::vector<Vector2> points;
    for (std::size_t i = 0; i < corner.size(); ++i) {
        const Vector2 arc{corner[i].x + inward[i].x * radius[i],
                          corner[i].y + inward[i].y * radius[i]};
        // The arc starts at 180, 270, 0 and 90 degrees and turns a quarter clockwise.
        const float start = std::numbers::pi_v<float> * (1.0f + 0.5f * static_cast<float>(i));
        for (int step = 0; step <= cornerSegments; ++step) {
            const float angle = start + std::numbers::pi_v<float> * 0.5f *
                                            static_cast<float>(step) / cornerSegments;
            points.push_back(
                Vector2{arc.x + radius[i] * std::cos(angle), arc.y + radius[i] * std::sin(angle)});
        }
    }
    return points;
}

void paintShoulder(std::string_view key, Vector2 centre, float box, Color ink) {
    const std::vector<Vector2> outline = shoulderOutline(centre, shoulderSide * box, key == "LB");
    const float stroke = shoulderStroke * box;
    for (std::size_t i = 0; i < outline.size(); ++i) {
        roundBar(outline[i], outline[(i + 1) % outline.size()], stroke, ink);
    }
    const TextStyle letters{shoulderCapHeight * box / capHeightPerEm};
    const float width = type().measure(key, letters);
    type().drawCentred(key, centre.x - width * 0.5f, centre.y, letters, ink);
}

TextStyle capLabelStyle(float box) {
    return TextStyle{pngCapHeight * box / capHeightPerEm};
}

float capWidth(const std::string& label, float box) {
    return std::max(2.0f * pngRingOuter * box,
                    type().measure(label, capLabelStyle(box)) + 2.0f * capLabelPadding * box);
}

} // namespace

std::optional<std::string> ButtonGlyphPainter::capLabel(std::string_view key) const {
    if (prompts_->device.current() != input::Device::KeyboardMouse) {
        return std::nullopt;
    }
    return prompts_->shortcuts.keyLabelForGlyph(key);
}

float ButtonGlyphPainter::advance(std::string_view key, float box) const {
    const std::optional<std::string> label = capLabel(key);
    if (!label) {
        return box;
    }
    // The slack around the ring that the square leaves, so a cap sits as a ring does.
    return capWidth(*label, box) + (box - 2.0f * pngRingOuter * box);
}

void ButtonGlyphPainter::paint(std::string_view key, Vector2 centre, float box, Color ink) const {
    if (const std::optional<std::string> label = capLabel(key)) {
        const float height = 2.0f * pngRingOuter * box;
        const float width = capWidth(*label, box);
        const Rectangle cap{centre.x - width * 0.5f, centre.y - height * 0.5f, width, height};
        DrawRectangleRoundedLinesEx(cap, capRoundness, capSegments, pngRingStroke * box, ink);
        const TextStyle text = capLabelStyle(box);
        const float textWidth = type().measure(*label, text);
        type().drawCentred(*label, centre.x - textWidth * 0.5f, centre.y, text, ink);
        return;
    }
    if (key == "LB" || key == "RB") {
        paintShoulder(key, centre, box, ink);
        return;
    }
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

} // namespace opensu::ui

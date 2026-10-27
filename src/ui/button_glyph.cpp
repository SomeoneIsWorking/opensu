#include "button_glyph.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "input/keyboard_bindings.hpp"
#include "input/shortcuts.hpp"
#include "interface_input.hpp"
#include "interface_text.hpp"

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

// A d-pad direction is the letter ring around a solid arrowhead, its tip this far from the centre
// and its base this wide, in ring radii.
constexpr float arrowReach = 0.5f;
constexpr float arrowHalfBase = 0.45f;

bool isShoulder(std::string_view key) {
    return key == "LB" || key == "RB" || key == "LT" || key == "RT";
}

/// The arrow's direction for a d-pad key, or (0, 0) for any other.
Point arrowOf(std::string_view key) {
    if (key == "UP") {
        return {0.0f, -1.0f};
    }
    if (key == "DOWN") {
        return {0.0f, 1.0f};
    }
    if (key == "LEFT") {
        return {-1.0f, 0.0f};
    }
    if (key == "RIGHT") {
        return {1.0f, 0.0f};
    }
    return {0.0f, 0.0f};
}

/// Whether `key` has a glyph shape of its own; the rest (L3, GUIDE) draw as a cap with their name.
bool hasShape(std::string_view key) {
    const Point arrow = arrowOf(key);
    return key.size() == 1 || isShoulder(key) || arrow.x != 0.0f || arrow.y != 0.0f;
}

void roundBar(Canvas& canvas, Point from, Point to, float stroke, Colour ink) {
    canvas.line(from, to, stroke, ink);
    canvas.fillCircle(from, stroke * 0.5f, ink);
    canvas.fillCircle(to, stroke * 0.5f, ink);
}

/// A full ring between `inner` and `outer`.
void ring(Canvas& canvas, Point centre, float inner, float outer, Colour ink) {
    canvas.ring(gpu2d::Ring{.centre = centre,
                            .inner = inner,
                            .outer = outer,
                            .startDegrees = 0.0f,
                            .endDegrees = 360.0f},
                ink);
}

/// The outline of a square of `side` about `centre` whose top corner on the `bigSide` has the large
/// radius, as points clockwise from the top-left.
std::vector<Point> shoulderOutline(Point centre, float side, bool bigOnLeft) {
    const float half = side * 0.5f;
    const float big = shoulderBigCorner * side;
    const float small = shoulderSmallCorner * side;
    // Radius per corner, clockwise from the top-left, and each corner's arc centre.
    const std::array<float, 4> radius{bigOnLeft ? big : small, bigOnLeft ? small : big, small,
                                      small};
    const std::array<Point, 4> corner{
        Point{centre.x - half, centre.y - half}, Point{centre.x + half, centre.y - half},
        Point{centre.x + half, centre.y + half}, Point{centre.x - half, centre.y + half}};
    const std::array<Point, 4> inward{Point{1.0f, 1.0f}, Point{-1.0f, 1.0f}, Point{-1.0f, -1.0f},
                                      Point{1.0f, -1.0f}};
    std::vector<Point> points;
    for (std::size_t i = 0; i < corner.size(); ++i) {
        const Point arc{corner[i].x + inward[i].x * radius[i],
                        corner[i].y + inward[i].y * radius[i]};
        // The arc starts at 180, 270, 0 and 90 degrees and turns a quarter clockwise.
        const float start = std::numbers::pi_v<float> * (1.0f + 0.5f * static_cast<float>(i));
        for (int step = 0; step <= cornerSegments; ++step) {
            const float angle = start + std::numbers::pi_v<float> * 0.5f *
                                            static_cast<float>(step) / cornerSegments;
            points.push_back(
                Point{arc.x + radius[i] * std::cos(angle), arc.y + radius[i] * std::sin(angle)});
        }
    }
    return points;
}

void paintShoulder(Canvas& canvas, gpu2d::ui::Typesetter& text, std::string_view key, Point centre,
                   float box, Colour ink) {
    const std::vector<Point> outline =
        shoulderOutline(centre, shoulderSide * box, key == "LB" || key == "LT");
    const float stroke = shoulderStroke * box;
    for (std::size_t i = 0; i < outline.size(); ++i) {
        roundBar(canvas, outline[i], outline[(i + 1) % outline.size()], stroke, ink);
    }
    const float letters = shoulderCapHeight * box / capHeightPerEm;
    const float width = text.measure(key, letters);
    text.drawCentred(canvas, key, Point{centre.x - width * 0.5f, centre.y}, letters, ink);
}

float capLabelSize(float box) {
    return pngCapHeight * box / capHeightPerEm;
}

float capWidth(gpu2d::ui::Typesetter& text, std::string_view label, float box) {
    return std::max(2.0f * pngRingOuter * box,
                    text.measure(label, capLabelSize(box)) + 2.0f * capLabelPadding * box);
}

/// `label` in a rounded outline the ring's height, as a key cap.
void paintCap(Canvas& canvas, gpu2d::ui::Typesetter& text, std::string_view label, Point centre,
              float box, Colour ink) {
    const float height = 2.0f * pngRingOuter * box;
    const float width = capWidth(text, label, box);
    const Rect cap{centre.x - width * 0.5f, centre.y - height * 0.5f, width, height};
    // The outline is drawn outside the cap's rectangle.
    const float stroke = pngRingStroke * box;
    canvas.strokeRoundRect(gpu2d::Rect{cap.x - stroke, cap.y - stroke, cap.width + 2.0f * stroke,
                                       cap.height + 2.0f * stroke},
                           cornerRadius(cap, capRoundness) + stroke, stroke, ink);
    const float size = capLabelSize(box);
    const float textWidth = text.measure(label, size);
    text.drawCentred(canvas, label, Point{centre.x - textWidth * 0.5f, centre.y}, size, ink);
}

void paintArrow(Canvas& canvas, Point direction, Point centre, float box, Colour ink) {
    const float outer = pngRingOuter * box;
    ring(canvas, centre, outer - pngRingStroke * box, outer, ink);
    const float reach = arrowReach * outer;
    const float half = arrowHalfBase * outer;
    // The base sits as far behind the centre as half the reach, so the head is centred.
    const Point tip{centre.x + direction.x * reach, centre.y + direction.y * reach};
    const Point base{centre.x - direction.x * reach * 0.5f, centre.y - direction.y * reach * 0.5f};
    const Point across{-direction.y * half, direction.x * half};
    canvas.triangle(tip, Point{base.x + across.x, base.y + across.y},
                    Point{base.x - across.x, base.y - across.y}, ink);
}

} // namespace

bool ButtonGlyphPainter::keys() const noexcept {
    return prompts_->device.current() == input::Device::KeyboardMouse;
}

std::optional<std::string> ButtonGlyphPainter::capLabel(std::string_view key) const {
    if (!keys()) {
        return std::nullopt;
    }
    return prompts_->shortcuts.keyLabelForGlyph(key);
}

float ButtonGlyphPainter::advance(std::string_view key, float box) const {
    const std::optional<std::string> label = capLabel(key);
    if (!label && hasShape(key)) {
        return box;
    }
    // The slack around the ring that the square leaves, so a cap sits as a ring does.
    return capWidth(text_, label ? std::string_view{*label} : key, box) +
           (box - 2.0f * pngRingOuter * box);
}

void ButtonGlyphPainter::paint(Canvas& canvas, std::string_view key, Point centre, float box,
                               Colour ink) const {
    if (const std::optional<std::string> label = capLabel(key)) {
        paintCap(canvas, text_, *label, centre, box, ink);
        return;
    }
    if (!hasShape(key)) {
        paintCap(canvas, text_, key, centre, box, ink);
        return;
    }
    if (const Point arrow = arrowOf(key); arrow.x != 0.0f || arrow.y != 0.0f) {
        paintArrow(canvas, arrow, centre, box, ink);
        return;
    }
    if (isShoulder(key)) {
        paintShoulder(canvas, text_, key, centre, box, ink);
        return;
    }
    if (key == "-" || key == "+") {
        const float unit = box / vectorViewport;
        const float radius = vectorRingRadius * unit;
        const float half = vectorRingStroke * unit * 0.5f;
        ring(canvas, centre, radius - half, radius + half, ink);
        const float bar = vectorBarHalf * unit;
        const float stroke = vectorBarStroke * unit;
        roundBar(canvas, {centre.x - bar, centre.y}, {centre.x + bar, centre.y}, stroke, ink);
        if (key == "+") {
            roundBar(canvas, {centre.x, centre.y - bar}, {centre.x, centre.y + bar}, stroke, ink);
        }
        return;
    }
    const float outer = pngRingOuter * box;
    ring(canvas, centre, outer - pngRingStroke * box, outer, ink);
    const float letter = pngCapHeight * box / capHeightPerEm;
    const float width = text_.measure(key, letter);
    text_.drawCentred(canvas, key, Point{centre.x - width * 0.5f, centre.y}, letter, ink);
}

std::string_view InterfacePrompts::glyph(gpu2d::ui::Action action) const {
    // A physical keyboard finishes a line with the key A is on.
    if (action == gpu2d::ui::Action::Submit && glyphs_.keys()) {
        return input::glyphOf(gamepad::Button::A);
    }
    return input::glyphOf(buttonOfInterfaceAction(action));
}

bool InterfacePrompts::bound(gpu2d::ui::Action action, bool typing) {
    if (!glyphs_.keys()) {
        // The pad types on an open keyboard, whose own row names its buttons.
        return !keyboard_.isOpen();
    }
    if (!typing) {
        return true;
    }
    return std::ranges::any_of(typingKeys, [action](const TypingKey& key) {
        return key.action == action;
    });
}

float InterfacePrompts::width(gpu2d::ui::Action action, float height) {
    return glyphs_.advance(glyph(action), height);
}

void InterfacePrompts::draw(gpu2d::Canvas& canvas, gpu2d::ui::Action action, const gpu2d::Rect& box,
                            gpu2d::Colour ink) {
    glyphs_.paint(canvas, glyph(action),
                  gpu2d::Point{box.x + box.width * 0.5f, box.y + box.height * 0.5f}, box.height,
                  ink);
}

} // namespace opensu::ui

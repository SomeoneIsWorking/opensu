#include "on_screen_keyboard_painter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr Colour dockFill{0xF8, 0xF8, 0xFB, 244};
constexpr Colour cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour outlineInk{0x4D, 0x46, 0x55, 255};
constexpr Colour wheelFill{0xE6, 0xE4, 0xEC, 255};
constexpr float keySp = 10.0f;
constexpr float miniSp = 7.5f;
constexpr float faceSp = 17.0f;
constexpr float stripSp = 11.0f;
constexpr float keyCornerDp = 4.0f;
constexpr float keyOutlineDp = 1.5f;
constexpr float stickGlyphDp = 14.0f;
constexpr float stripGlyphDp = 17.0f;
constexpr float pillPaddingDp = 6.0f;
constexpr float pillGapDp = 3.0f;
constexpr float stripGapDp = 6.0f;
constexpr float groupGapDp = 14.0f;
/// How far a petal's miniature characters sit from its centre, as a share of its radius.
constexpr float miniOffsetShare = 0.5f;
/// How large a face button glyph is drawn in an empty diamond face, as a share of its radius.
constexpr float emptyGlyphShare = 1.2f;
/// The face buttons' glyphs in `DaisyFace` order.
constexpr std::array<std::string_view, daisyFaces> faceGlyphs{"X", "Y", "B", "A"};

Point centreOf(const Rect& rect) noexcept {
    return Point{rect.centreX(), rect.centreY()};
}

/// The first character `face` types on `petal`, as text.
std::string characterOn(const OnScreenKeyboard& keyboard, std::size_t petal, DaisyFace face) {
    return std::string(1, daisyCharacter(keyboard.set(), petal, face));
}

} // namespace

std::vector<HintPrompt> OnScreenKeyboardPainter::hints(const OnScreenKeyboard& keyboard) const {
    // A physical keyboard types straight into the field, so it needs no key prompts.
    if (prompts_->device.current() == input::Device::KeyboardMouse) {
        return {{"A", "Done"}, {"B", "Close"}};
    }
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        std::vector<HintPrompt> wheel;
        if (!keyboard.petal()) {
            wheel.emplace_back("B", "Close");
        }
        wheel.emplace_back("-", "Clear");
        wheel.emplace_back("+", "Done");
        return wheel;
    }
    return {{"A", "Type"},
            {"B", "Delete"},
            {"X", keyboard.shifted() ? "Caps on" : "Caps"},
            {"Y", "Space"},
            {"-", "Clear"},
            {"+", "Done"}};
}

void OnScreenKeyboardPainter::paint(Canvas& canvas, const OnScreenKeyboard& keyboard, Point size,
                                    float dp, const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const KeyboardLayout layout =
        layoutKeyboard(Rect{0.0f, 0.0f, size.x, size.y}, dp, keyboard.style());
    const float alpha = look.alpha;
    const PanelScope scope{canvas, centreOf(layout.panel), look.scale};
    canvas.fillRoundRect(onCanvas(layout.panel), layout.panelRadius, withAlpha(dockFill, alpha));
    const std::vector<HintPrompt> own = hints(keyboard);
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        paintWheel(canvas, keyboard, layout, alpha);
        const float width = paintStrip(canvas, keyboard, {}, layout.unit, alpha, false);
        const float unit = layout.unit * std::min(1.0f, layout.strip.width / width);
        paintStrip(canvas, keyboard, centreOf(layout.strip), unit, alpha, true);
        paintHints(canvas, own, layout.hints, layout.unit, alpha);
        return;
    }
    paintGrid(canvas, keyboard, layout, alpha);
    // The grid's hints take two lines, the longer half first.
    const std::size_t half = (own.size() + 1) / 2;
    paintHints(canvas, std::span{own}.first(half), layout.strip, layout.unit, alpha);
    paintHints(canvas, std::span{own}.subspan(half), layout.hints, layout.unit, alpha);
}

void OnScreenKeyboardPainter::paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                        const KeyboardLayout& layout, float alpha) const {
    const float unit = layout.unit;
    const float cap = keySp * unit;
    const std::span<const GridKey> keys = gridKeys();
    for (std::size_t i = 0; i < keys.size() && i < layout.keys.size(); ++i) {
        const Rect& box = layout.keys[i];
        const bool focused = i == keyboard.keyFocus();
        const bool done = keys[i].kind == KeyKind::Done;
        const float radius = keyCornerDp * unit;
        canvas.fillRoundRect(onCanvas(box), radius, withAlpha(done ? outlineInk : cardFill, alpha));
        if (focused) {
            canvas.strokeRoundRect(onCanvas(box), radius, keyOutlineDp * unit,
                                   withAlpha(done ? palette::ink : outlineInk, alpha));
        }
        const Colour ink = withAlpha(done ? white : palette::ink, alpha);
        if (keys[i].kind == KeyKind::Character) {
            text_.drawInkCentred(canvas, keys[i].label, centreOf(box), cap, ink);
        } else {
            text_.drawCentred(canvas, keys[i].label, Point{box.centreX() - text_.measure(keys[i].label, cap) * 0.5f, box.centreY()}, cap, ink);
        }
    }
}

void OnScreenKeyboardPainter::paintWheel(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                         const KeyboardLayout& layout, float alpha) const {
    const float unit = layout.unit;
    const std::optional<std::size_t> picked = keyboard.petal();
    const bool pointed = keyboard.pointed();
    canvas.fillCircle(centreOf(layout.ring), layout.ring.width * 0.5f, withAlpha(wheelFill, alpha));
    glyphs_.paint(canvas, "L3", centreOf(layout.ring), stickGlyphDp * unit,
                  withAlpha(palette::inkSoft, alpha));

    const float mini = miniSp * unit;
    for (std::size_t petal = 0; petal < daisyPetals; ++petal) {
        const Rect bounds = layout.petalBounds(petal);
        const bool chosen = picked == petal;
        canvas.fillCircle(centreOf(bounds), layout.petalRadius,
                          withAlpha(chosen ? outlineInk : cardFill, alpha));
        if (pointed && keyboard.keyFocus() == petal && !chosen) {
            canvas.ring(gpu2d::Ring{.centre = centreOf(bounds),
                                    .inner = layout.petalRadius - keyOutlineDp * unit,
                                    .outer = layout.petalRadius,
                                    .startDegrees = 0.0f,
                                    .endDegrees = 360.0f},
                        withAlpha(outlineInk, alpha));
        }
        for (std::size_t face = 0; face < daisyFaces; ++face) {
            const auto shown = static_cast<DaisyFace>(face);
            const auto [stepX, stepY] = faceStep(shown);
            const float offset = layout.petalRadius * miniOffsetShare;
            text_.drawInkCentred(
                canvas, characterOn(keyboard, petal, shown),
                Point{bounds.centreX() + stepX * offset, bounds.centreY() + stepY * offset}, mini,
                withAlpha(chosen ? white : palette::ink, alpha));
        }
    }

    // The picked petal's characters where the face buttons that type them sit.
    const float large = faceSp * unit;
    for (std::size_t face = 0; face < daisyFaces; ++face) {
        const auto shown = static_cast<DaisyFace>(face);
        const Rect bounds = layout.faceBounds(shown);
        if (!picked) {
            canvas.fillCircle(centreOf(bounds), layout.faceRadius, withAlpha(wheelFill, alpha));
            glyphs_.paint(canvas, faceGlyphs[face], centreOf(bounds),
                          layout.faceRadius * emptyGlyphShare, withAlpha(palette::inkSoft, alpha));
            continue;
        }
        const bool focused = pointed && keyboard.keyFocus() == daisyPetals + face;
        canvas.fillCircle(centreOf(bounds), layout.faceRadius,
                          withAlpha(focused ? outlineInk : cardFill, alpha));
        text_.drawInkCentred(canvas, characterOn(keyboard, *picked, shown), centreOf(bounds),
                                 large, withAlpha(focused ? white : palette::ink, alpha));
    }
}

float OnScreenKeyboardPainter::paintStrip(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                          Point centre, float unit, float alpha, bool draw) const {
    const float label = stripSp * unit;
    const float glyph = stripGlyphDp * unit;
    const float pillHeight = glyph;
    const Colour ink = withAlpha(outlineInk, alpha);
    std::array<float, daisySets> pills{};
    float width = glyphs_.advance("LT", glyph) + glyphs_.advance("RT", glyph);
    for (std::size_t set = 0; set < daisySets; ++set) {
        pills[set] = text_.measure(daisySetLabel(static_cast<DaisySet>(set)), label) +
                     2.0f * pillPaddingDp * unit;
        width += pills[set] + pillGapDp * unit;
    }
    width += 2.0f * stripGapDp * unit - pillGapDp * unit;
    const std::array<HintPrompt, 2> shoulders{{{"LB", "Delete"}, {"RB", "Space"}}};
    for (const auto& [key, action] : shoulders) {
        width += groupGapDp * unit + glyphs_.advance(key, glyph) + stripGapDp * unit +
                 text_.measure(action, label);
    }
    if (!draw) {
        return width;
    }
    float x = centre.x - width * 0.5f;
    const auto glyphAt = [&](std::string_view key) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(canvas, key, Point{x + advance * 0.5f, centre.y}, glyph, ink);
        x += advance;
    };
    glyphAt("LT");
    x += stripGapDp * unit;
    for (std::size_t set = 0; set < daisySets; ++set) {
        const auto shown = static_cast<DaisySet>(set);
        const bool current = shown == keyboard.set();
        const Rect pill{x, centre.y - pillHeight * 0.5f, pills[set], pillHeight};
        canvas.fillRoundRect(onCanvas(pill), pillHeight * 0.5f,
                             withAlpha(current ? outlineInk : cardFill, alpha));
        const std::string_view text = daisySetLabel(shown);
        text_.drawCentred(canvas, text, Point{pill.centreX() - text_.measure(text, label) * 0.5f, centre.y}, label, withAlpha(current ? white : palette::ink, alpha));
        x += pills[set] + pillGapDp * unit;
    }
    x += stripGapDp * unit - pillGapDp * unit;
    glyphAt("RT");
    for (const auto& [key, action] : shoulders) {
        x += groupGapDp * unit;
        glyphAt(key);
        x += stripGapDp * unit;
        text_.drawCentred(canvas, action, Point{x, centre.y}, label, ink);
        x += text_.measure(action, label);
    }
    return width;
}

void OnScreenKeyboardPainter::paintHints(Canvas& canvas, std::span<const HintPrompt> hints,
                                         const Rect& line, float unit, float alpha) const {
    const float width = hintRowWidth(glyphs_, text_, hints, unit);
    const float fitted = width > line.width ? unit * line.width / width : unit;
    paintHintRow(canvas, glyphs_, text_, hints, centreOf(line), fitted,
                 withAlpha(outlineInk, alpha));
}

} // namespace opensu::ui

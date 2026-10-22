#include "on_screen_keyboard_painter.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour outlineInk{0x4D, 0x46, 0x55, 255};
constexpr Colour wheelFill{0xEC, 0xEB, 0xF1, 255};
constexpr float fieldSp = 18.0f;
constexpr float keySp = 16.0f;
constexpr float petalSp = 14.0f;
constexpr float sideSp = 14.0f;
constexpr float insetDp = 14.0f;
constexpr float cornerDp = 10.0f;
constexpr float outlineDp = 1.8f;
constexpr float iconDp = 9.0f;
constexpr float legendGlyphDp = 14.0f;
constexpr float legendOffsetDp = 16.0f;
constexpr float sideGlyphDp = 20.0f;
constexpr float sideRowDp = 26.0f;
constexpr float sideGapDp = 8.0f;
/// How much larger the picked petal is drawn.
constexpr float pickedGrowth = 1.12f;
constexpr double caretPeriod = 1.0;

/// One dot for each character of a password.
std::string dotsFor(std::string_view secret) {
    std::string dots;
    for (const char byte : secret) {
        if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
            dots += "•";
        }
    }
    return dots;
}

/// A magnifying glass: a ring and a handle.
void paintGlass(Canvas& canvas, Point centre, float dp, Colour ink) {
    const float radius = iconDp * dp * 0.55f;
    const Point ringCentre{centre.x - radius * 0.35f, centre.y - radius * 0.35f};
    canvas.ring(gpu2d::Ring{.centre = ringCentre,
                            .inner = radius - dp * 0.9f,
                            .outer = radius,
                            .startDegrees = 0.0f,
                            .endDegrees = 360.0f},
                ink);
    canvas.line(Point{ringCentre.x + radius * 0.7f, ringCentre.y + radius * 0.7f},
                Point{centre.x + radius * 1.0f, centre.y + radius * 1.0f}, dp * 1.8f, ink);
}

/// A filled circle filling `bounds`, grown by `scale` about its centre.
RoundRect circleIn(const Rect& bounds, float scale) {
    const float radius = bounds.width * 0.5f * scale;
    return RoundRect{
        Rect{bounds.centreX() - radius, bounds.centreY() - radius, 2.0f * radius, 2.0f * radius},
        radius};
}

void fillShape(Canvas& canvas, const RoundRect& shape, Colour colour, float alpha) {
    fillRoundRect(
        canvas, shape,
        [colour, alpha](Point, float) {
            return withAlpha(colour, alpha);
        },
        Tessellation{16, 1});
}

} // namespace

void OnScreenKeyboardPainter::paintBar(Canvas& canvas, const Rect& rect, bool focused, float dp,
                                       float alpha) {
    const RoundRect bar{rect, cornerDp * dp};
    fillRoundRect(canvas, bar, [alpha](Point, float) {
        return withAlpha(cardFill, alpha);
    });
    if (focused) {
        fillBand(canvas, bar, bar.grown(-outlineDp * dp), [alpha](Point, float) {
            return withAlpha(outlineInk, alpha);
        });
    }
}

void OnScreenKeyboardPainter::drawFitted(Canvas& canvas, std::string text, float x, float centreY,
                                         float room, const TextStyle& style, Colour colour) const {
    while (text.size() > 1 && typeface_.measure(text, style) > room) {
        std::size_t cut = text.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) {
            --cut;
        }
        text.resize(cut);
        if (typeface_.measure(text + "...", style) <= room) {
            text += "...";
            break;
        }
    }
    typeface_.drawCentred(canvas, text, x, centreY, style, colour);
}

std::vector<HintPrompt> OnScreenKeyboardPainter::hints(const OnScreenKeyboard& keyboard) const {
    // A physical keyboard types straight into the field, so it needs no key prompts.
    if (prompts_->device.current() == input::Device::KeyboardMouse) {
        return {{"A", "Done"}, {"B", "Close"}};
    }
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        std::vector<HintPrompt> wheel{{"LEFT", "Cursor"}};
        if (!keyboard.petal()) {
            wheel.emplace_back("B", "Close");
        }
        wheel.insert(wheel.end(), {{"-", "Clear"}, {"+", "Done"}});
        return wheel;
    }
    std::vector<HintPrompt> grid{{"A", "Type"}, {"B", "Delete"}};
    if (keyboard.secret()) {
        grid.emplace_back("X", keyboard.shifted() ? "Caps on" : "Caps");
    }
    grid.insert(grid.end(), {{"Y", "Space"}, {"-", "Clear"}, {"+", "Done"}});
    return grid;
}

void OnScreenKeyboardPainter::paint(Canvas& canvas, const OnScreenKeyboard& keyboard, Point size,
                                    float dp, const PanelLook& look, double seconds) const {
    paint(canvas, keyboard, layoutKeyboard(Rect{0.0f, 0.0f, size.x, size.y}, dp, keyboard.style()),
          size, dp, look, seconds, {});
}

void OnScreenKeyboardPainter::paint(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                    const KeyboardLayout& layout, Point size, float dp,
                                    const PanelLook& look, double seconds,
                                    const KeyboardExtras& extras) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, alpha));
    const PanelScope scope{canvas, Point{layout.panel.centreX(), layout.panel.centreY()},
                           look.scale};
    fillRoundRect(canvas, RoundRect{layout.panel, layout.panelRadius}, [alpha](Point, float) {
        return withAlpha(panelFill, alpha);
    });
    paintField(canvas, keyboard, layout.field, dp, alpha, seconds);
    if (extras.reserved) {
        extras.reserved(alpha);
    }
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        paintWheel(canvas, keyboard, layout, dp, alpha);
    } else {
        paintGrid(canvas, keyboard, layout, dp, alpha, extras.keysFocused);
    }
    const std::vector<HintPrompt> own = extras.hints.empty() ? hints(keyboard) : extras.hints;
    paintHintRow(canvas, glyphs_, typeface_, own,
                 Point{layout.hints.centreX(), layout.hints.centreY()}, dp,
                 withAlpha(outlineInk, alpha));
}

void OnScreenKeyboardPainter::paintField(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                         const Rect& field, float dp, float alpha,
                                         double seconds) const {
    paintBar(canvas, field, false, dp, alpha);
    const float inset = insetDp * dp;
    const bool secret = keyboard.secret();
    if (!secret) {
        paintGlass(canvas, Point{field.x + inset + iconDp * dp * 0.5f, field.centreY()}, dp,
                   withAlpha(palette::inkSoft, alpha));
    }
    const TextStyle style{fieldSp * dp};
    const float textX = field.x + inset * (secret ? 1.0f : 2.0f) + (secret ? 0.0f : iconDp * dp);
    const float room = field.right() - inset - textX;
    const std::string& text = keyboard.text();
    const std::string shown = secret ? dotsFor(text) : text;
    if (shown.empty()) {
        drawFitted(canvas, keyboard.prompt(), textX, field.centreY(), room, style,
                   withAlpha(palette::inkSoft, alpha));
    } else {
        drawFitted(canvas, shown, textX, field.centreY(), room, style,
                   withAlpha(palette::ink, alpha));
    }
    if (std::fmod(seconds, caretPeriod) >= caretPeriod * 0.5) {
        return;
    }
    const std::string_view before = std::string_view{text}.substr(0, keyboard.cursor());
    const std::string leading = secret ? dotsFor(before) : std::string{before};
    const float caretX =
        textX + (leading.empty() ? 0.0f : std::min(typeface_.measure(leading, style), room));
    const float caretHeight = typeface_.lineBox(style) * 0.8f;
    canvas.fillRect(
        gpu2d::Rect{caretX + dp, field.centreY() - caretHeight * 0.5f, 1.6f * dp, caretHeight},
        withAlpha(palette::ink, alpha));
}

void OnScreenKeyboardPainter::paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                        const KeyboardLayout& layout, float dp, float alpha,
                                        bool focusedKeys) const {
    const TextStyle cap{keySp * dp};
    const std::span<const GridKey> keys = gridKeys();
    for (std::size_t i = 0; i < keys.size() && i < layout.keys.size(); ++i) {
        const bool focused = focusedKeys && i == keyboard.keyFocus();
        const bool done = keys[i].kind == KeyKind::Done;
        const RoundRect keyShape{layout.keys[i], cornerDp * dp};
        paintBar(canvas, layout.keys[i], focused && !done, dp, alpha);
        if (done) {
            fillRoundRect(canvas, keyShape, [alpha](Point, float) {
                return withAlpha(outlineInk, alpha);
            });
            if (focused) {
                fillBand(canvas, keyShape, keyShape.grown(-outlineDp * dp), [alpha](Point, float) {
                    return withAlpha(palette::ink, alpha);
                });
            }
        }
        const std::string label{keys[i].label};
        const float width = typeface_.measure(label, cap);
        typeface_.drawCentred(canvas, label, layout.keys[i].centreX() - width * 0.5f,
                              layout.keys[i].centreY(), cap,
                              withAlpha(done ? white : palette::ink, alpha));
    }
}

void OnScreenKeyboardPainter::paintWheel(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                         const KeyboardLayout& layout, float dp,
                                         float alpha) const {
    fillShape(canvas, circleIn(layout.wheel, 1.0f), wheelFill, alpha);
    const std::optional<std::size_t> picked = keyboard.petal();
    const TextStyle character{petalSp * dp};
    const Colour ink = withAlpha(outlineInk, alpha);
    for (std::size_t petal = 0; petal < daisyPetals; ++petal) {
        const bool chosen = picked == petal;
        const RoundRect disc = circleIn(layout.petalBounds(petal), chosen ? pickedGrowth : 1.0f);
        fillShape(canvas, disc, chosen ? outlineInk : cardFill, alpha);
        for (std::size_t face = 0; face < daisyFaces; ++face) {
            const std::size_t index = petal * daisyFaces + face;
            const Rect& cell = layout.keys[index];
            if (keyboard.pointed() && keyboard.keyFocus() == index) {
                fillShape(canvas, circleIn(cell, 1.15f), chosen ? cardFill : outlineInk, alpha);
            }
            const bool light = chosen != (keyboard.pointed() && keyboard.keyFocus() == index);
            const char typed = daisyCharacter(keyboard.set(), petal, static_cast<DaisyFace>(face));
            const std::string_view label{&typed, 1};
            typeface_.drawCentred(
                canvas, label, cell.centreX() - typeface_.measure(label, character) * 0.5f,
                cell.centreY(), character, withAlpha(light ? white : palette::ink, alpha));
        }
    }

    // The face buttons in the places they type on every petal.
    const float legend = legendGlyphDp * dp;
    const float centreX = layout.wheel.centreX();
    const float centreY = layout.wheel.centreY();
    const float offset = legendOffsetDp * dp;
    glyphs_.paint(canvas, "X", Point{centreX - offset, centreY}, legend, ink);
    glyphs_.paint(canvas, "Y", Point{centreX, centreY - offset}, legend, ink);
    glyphs_.paint(canvas, "B", Point{centreX + offset, centreY}, legend, ink);
    glyphs_.paint(canvas, "A", Point{centreX, centreY + offset}, legend, ink);

    // The sets on the left, between LT and RT; the shoulder buttons' actions on the right.
    const TextStyle side{sideSp * dp};
    const float glyph = sideGlyphDp * dp;
    const float row = sideRowDp * dp;
    const float leftCentre = (layout.panel.x + layout.wheel.x) * 0.5f;
    float y = centreY - row * 2.5f;
    glyphs_.paint(canvas, "LT", Point{leftCentre, y}, glyph, ink);
    for (std::size_t set = 0; set < daisySets; ++set) {
        y += row;
        const auto shown = static_cast<DaisySet>(set);
        const std::string_view label = daisySetLabel(shown);
        const float width = typeface_.measure(label, side);
        const Rect pill{leftCentre - width * 0.5f - 10.0f * dp, y - row * 0.4f, width + 20.0f * dp,
                        row * 0.8f};
        const bool current = shown == keyboard.set();
        if (current) {
            fillShape(canvas, RoundRect{pill, pill.height * 0.5f}, outlineInk, alpha);
        }
        typeface_.drawCentred(canvas, label, leftCentre - width * 0.5f, y, side,
                              withAlpha(current ? white : palette::ink, alpha));
    }
    glyphs_.paint(canvas, "RT", Point{leftCentre, y + row}, glyph, ink);

    const float rightX = layout.wheel.right() + sideGapDp * dp * 2.0f;
    const std::array<HintPrompt, 2> shoulders{{{"LB", "Delete"}, {"RB", "Space"}}};
    y = centreY - row * 0.5f;
    for (const auto& [key, label] : shoulders) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(canvas, key, Point{rightX + advance * 0.5f, y}, glyph, ink);
        typeface_.drawCentred(canvas, label, rightX + advance + sideGapDp * dp, y, side, ink);
        y += row;
    }
}

} // namespace opensu::ui

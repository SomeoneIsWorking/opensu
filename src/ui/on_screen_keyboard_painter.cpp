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
/// A field 40 dp tall draws its text at 18 sp, 14 dp in from its edges, with a 10 dp corner.
constexpr float fieldTextShare = 0.45f;
constexpr float fieldInsetShare = 0.35f;
constexpr float fieldCornerShare = 0.25f;
constexpr float fieldIconShare = 0.225f;
constexpr float keySp = 10.0f;
constexpr float miniSp = 7.5f;
constexpr float faceSp = 17.0f;
constexpr float stripSp = 11.0f;
constexpr float cornerDp = 10.0f;
constexpr float keyCornerDp = 4.0f;
constexpr float outlineDp = 1.8f;
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
constexpr double caretPeriod = 1.0;
/// The face buttons' glyphs in `DaisyFace` order.
constexpr std::array<std::string_view, daisyFaces> faceGlyphs{"X", "Y", "B", "A"};

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

/// A magnifying glass `size` across: a ring and a handle.
void paintGlass(Canvas& canvas, Point centre, float size, Colour ink) {
    const float radius = size * 0.55f;
    const float stroke = size * 0.1f;
    const Point ringCentre{centre.x - radius * 0.35f, centre.y - radius * 0.35f};
    canvas.ring(gpu2d::Ring{.centre = ringCentre,
                            .inner = radius - stroke,
                            .outer = radius,
                            .startDegrees = 0.0f,
                            .endDegrees = 360.0f},
                ink);
    canvas.line(Point{ringCentre.x + radius * 0.7f, ringCentre.y + radius * 0.7f},
                Point{centre.x + radius, centre.y + radius}, stroke * 2.0f, ink);
}

Point centreOf(const Rect& rect) noexcept {
    return Point{rect.centreX(), rect.centreY()};
}

/// The first character `face` types on `petal`, as text.
std::string characterOn(const OnScreenKeyboard& keyboard, std::size_t petal, DaisyFace face) {
    return std::string(1, daisyCharacter(keyboard.set(), petal, face));
}

} // namespace

void OnScreenKeyboardPainter::paintBar(Canvas& canvas, const Rect& rect, bool focused, float dp,
                                       float alpha) {
    const float radius = std::min(cornerDp * dp, rect.height * 0.5f);
    canvas.fillRoundRect(onCanvas(rect), radius, withAlpha(cardFill, alpha));
    if (focused) {
        canvas.strokeRoundRect(onCanvas(rect), radius, outlineDp * dp,
                               withAlpha(outlineInk, alpha));
    }
}

std::vector<HintPrompt> OnScreenKeyboardPainter::hints(const OnScreenKeyboard& keyboard) const {
    // A physical keyboard types straight into the line, so it needs no key prompts.
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
          look, seconds, {});
}

void OnScreenKeyboardPainter::paint(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                    const KeyboardLayout& layout, const PanelLook& look,
                                    double seconds, const KeyboardExtras& extras) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    const PanelScope scope{canvas, centreOf(layout.panel), look.scale};
    canvas.fillRoundRect(onCanvas(layout.panel), layout.panelRadius, withAlpha(dockFill, alpha));
    if (layout.preview.height > 0.0f) {
        paintField(canvas, keyboard, layout.preview, alpha, seconds);
    }
    const std::vector<HintPrompt> own = extras.hints.empty() ? hints(keyboard) : extras.hints;
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        paintWheel(canvas, keyboard, layout, alpha);
        const float width = paintStrip(canvas, keyboard, {}, layout.unit, alpha, false);
        const float unit = layout.unit * std::min(1.0f, layout.strip.width / width);
        paintStrip(canvas, keyboard, centreOf(layout.strip), unit, alpha, true);
        paintHints(canvas, own, layout.hints, layout.unit, alpha);
        return;
    }
    paintGrid(canvas, keyboard, layout, alpha, extras.keysFocused);
    // The grid's hints take two lines, the longer half first.
    const std::size_t half = (own.size() + 1) / 2;
    paintHints(canvas, std::span{own}.first(half), layout.strip, layout.unit, alpha);
    paintHints(canvas, std::span{own}.subspan(half), layout.hints, layout.unit, alpha);
}

void OnScreenKeyboardPainter::paintField(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                         const Rect& field, float alpha, double seconds) const {
    const float height = field.height;
    canvas.fillRoundRect(onCanvas(field), height * fieldCornerShare, withAlpha(cardFill, alpha));
    const float inset = height * fieldInsetShare;
    const float icon = height * fieldIconShare;
    const bool secret = keyboard.secret();
    if (!secret) {
        paintGlass(canvas, Point{field.x + inset + icon * 0.5f, field.centreY()}, icon,
                   withAlpha(palette::inkSoft, alpha));
    }
    const TextStyle style{height * fieldTextShare};
    const float textX = field.x + inset * (secret ? 1.0f : 2.0f) + (secret ? 0.0f : icon);
    const float room = field.right() - inset - textX;
    const std::string& text = keyboard.text();
    const std::string shown = secret ? dotsFor(text) : text;
    // The end of a long line, where the cursor usually is, stays in view.
    const float offset = std::max(0.0f, typeface_.measure(shown, style) - room);
    if (shown.empty()) {
        typeface_.drawCentred(canvas, typeface_.fitted(keyboard.prompt(), room, style), textX,
                              field.centreY(), style, withAlpha(palette::inkSoft, alpha));
    } else {
        const ScopedClip clip{canvas, Rect{textX, field.y, room, field.height}};
        typeface_.drawCentred(canvas, shown, textX - offset, field.centreY(), style,
                              withAlpha(palette::ink, alpha));
    }
    if (std::fmod(seconds, caretPeriod) >= caretPeriod * 0.5) {
        return;
    }
    const std::string_view before = std::string_view{text}.substr(0, keyboard.cursor());
    const std::string leading = secret ? dotsFor(before) : std::string{before};
    const float caretX = textX - offset + typeface_.measure(leading, style);
    const float caretHeight = typeface_.lineBox(style) * 0.8f;
    canvas.fillRect(gpu2d::Rect{caretX + height * 0.025f, field.centreY() - caretHeight * 0.5f,
                                height * 0.04f, caretHeight},
                    withAlpha(palette::ink, alpha));
}

void OnScreenKeyboardPainter::paintGrid(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                        const KeyboardLayout& layout, float alpha,
                                        bool focusedKeys) const {
    const float unit = layout.unit;
    const TextStyle cap{keySp * unit};
    const std::span<const GridKey> keys = gridKeys();
    for (std::size_t i = 0; i < keys.size() && i < layout.keys.size(); ++i) {
        const Rect& box = layout.keys[i];
        const bool focused = focusedKeys && i == keyboard.keyFocus();
        const bool done = keys[i].kind == KeyKind::Done;
        const float radius = keyCornerDp * unit;
        canvas.fillRoundRect(onCanvas(box), radius, withAlpha(done ? outlineInk : cardFill, alpha));
        if (focused) {
            canvas.strokeRoundRect(onCanvas(box), radius, keyOutlineDp * unit,
                                   withAlpha(done ? palette::ink : outlineInk, alpha));
        }
        const Colour ink = withAlpha(done ? white : palette::ink, alpha);
        if (keys[i].kind == KeyKind::Character) {
            typeface_.drawInkCentred(canvas, keys[i].label, centreOf(box), cap, ink);
        } else {
            typeface_.drawCentred(canvas, keys[i].label,
                                  box.centreX() - typeface_.measure(keys[i].label, cap) * 0.5f,
                                  box.centreY(), cap, ink);
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

    const TextStyle mini{miniSp * unit};
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
            typeface_.drawInkCentred(
                canvas, characterOn(keyboard, petal, shown),
                Point{bounds.centreX() + stepX * offset, bounds.centreY() + stepY * offset}, mini,
                withAlpha(chosen ? white : palette::ink, alpha));
        }
    }

    // The picked petal's characters where the face buttons that type them sit.
    const TextStyle large{faceSp * unit};
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
        typeface_.drawInkCentred(canvas, characterOn(keyboard, *picked, shown), centreOf(bounds),
                                 large, withAlpha(focused ? white : palette::ink, alpha));
    }
}

float OnScreenKeyboardPainter::paintStrip(Canvas& canvas, const OnScreenKeyboard& keyboard,
                                          Point centre, float unit, float alpha, bool draw) const {
    const TextStyle label{stripSp * unit};
    const float glyph = stripGlyphDp * unit;
    const float pillHeight = glyph;
    const Colour ink = withAlpha(outlineInk, alpha);
    std::array<float, daisySets> pills{};
    float width = glyphs_.advance("LT", glyph) + glyphs_.advance("RT", glyph);
    for (std::size_t set = 0; set < daisySets; ++set) {
        pills[set] = typeface_.measure(daisySetLabel(static_cast<DaisySet>(set)), label) +
                     2.0f * pillPaddingDp * unit;
        width += pills[set] + pillGapDp * unit;
    }
    width += 2.0f * stripGapDp * unit - pillGapDp * unit;
    const std::array<HintPrompt, 2> shoulders{{{"LB", "Delete"}, {"RB", "Space"}}};
    for (const auto& [key, action] : shoulders) {
        width += groupGapDp * unit + glyphs_.advance(key, glyph) + stripGapDp * unit +
                 typeface_.measure(action, label);
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
        typeface_.drawCentred(canvas, text, pill.centreX() - typeface_.measure(text, label) * 0.5f,
                              centre.y, label, withAlpha(current ? white : palette::ink, alpha));
        x += pills[set] + pillGapDp * unit;
    }
    x += stripGapDp * unit - pillGapDp * unit;
    glyphAt("RT");
    for (const auto& [key, action] : shoulders) {
        x += groupGapDp * unit;
        glyphAt(key);
        x += stripGapDp * unit;
        typeface_.drawCentred(canvas, action, x, centre.y, label, ink);
        x += typeface_.measure(action, label);
    }
    return width;
}

void OnScreenKeyboardPainter::paintHints(Canvas& canvas, std::span<const HintPrompt> hints,
                                         const Rect& line, float unit, float alpha) const {
    const float width = hintRowWidth(glyphs_, typeface_, hints, unit);
    const float fitted = width > line.width ? unit * line.width / width : unit;
    paintHintRow(canvas, glyphs_, typeface_, hints, centreOf(line), fitted,
                 withAlpha(outlineInk, alpha));
}

} // namespace opensu::ui

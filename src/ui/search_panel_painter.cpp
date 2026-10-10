#include "search_panel_painter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "hud.hpp"
#include "panel_scope.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Color cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Color outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float fieldSp = 18.0f;
constexpr float resultSp = 16.0f;
constexpr float keySp = 16.0f;
constexpr float hintSp = 13.0f;
constexpr float hintGlyphDp = 18.0f;
constexpr float hintGapDp = 5.0f;
constexpr float hintSpacingDp = 14.0f;
constexpr float insetDp = 14.0f;
constexpr float cornerDp = 10.0f;
constexpr float outlineDp = 1.8f;
constexpr float iconDp = 9.0f;
constexpr double caretPeriod = 1.0;

/// Draws `text` cut to `room` pixels, ending in dots when it is cut.
void drawFitted(Typeface& typeface, std::string text, float x, float centreY, float room,
                const TextStyle& style, Color colour) {
    while (text.size() > 1 && typeface.measure(text, style) > room) {
        std::size_t cut = text.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) {
            --cut;
        }
        text.resize(cut);
        if (typeface.measure(text + "...", style) <= room) {
            text += "...";
            break;
        }
    }
    typeface.drawCentred(text, x, centreY, style, colour);
}

/// One dot for each character of a password.
std::string dotsFor(const std::string& secret) {
    std::string dots;
    for (const char byte : secret) {
        if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
            dots += "\u2022";
        }
    }
    return dots;
}

/// A white rounded bar; outlined when `focused`.
void paintBar(const Rect& rect, bool focused, float dp, float alpha) {
    const RoundRect bar{rect, cornerDp * dp};
    fillRoundRect(bar, [alpha](Vector2, float) {
        return withAlpha(cardFill, alpha);
    });
    if (focused) {
        fillBand(bar, bar.grown(-outlineDp * dp), [alpha](Vector2, float) {
            return withAlpha(outlineInk, alpha);
        });
    }
}

/// A magnifying glass: a ring and a handle.
void paintGlass(Vector2 centre, float dp, Color ink) {
    const float radius = iconDp * dp * 0.55f;
    const Vector2 ringCentre{centre.x - radius * 0.35f, centre.y - radius * 0.35f};
    DrawRing(ringCentre, radius - dp * 0.9f, radius, 0.0f, 360.0f, 24, ink);
    DrawLineEx(Vector2{ringCentre.x + radius * 0.7f, ringCentre.y + radius * 0.7f},
               Vector2{centre.x + radius * 1.0f, centre.y + radius * 1.0f}, dp * 1.8f, ink);
}

} // namespace

void SearchPanelPainter::paint(const SearchPanel& panel, Vector2 size, float dp,
                               const PanelLook& look, double seconds) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, alpha));
    const SearchLayout layout = layoutSearch(Rect{0.0f, 0.0f, size.x, size.y}, dp, panel.lists());
    const PanelScope scope{Vector2{layout.panel.centreX(), layout.panel.centreY()}, look.scale};
    fillRoundRect(RoundRect{layout.panel, layout.panelRadius}, [alpha](Vector2, float) {
        return withAlpha(panelFill, alpha);
    });

    // The field: the glass, then the text (or what to type) and the caret.
    paintBar(layout.field, false, dp, alpha);
    const float inset = insetDp * dp;
    if (!panel.secret()) {
        paintGlass(Vector2{layout.field.x + inset + iconDp * dp * 0.5f, layout.field.centreY()}, dp,
                   withAlpha(palette::inkSoft, alpha));
    }
    const TextStyle field{fieldSp * dp};
    const float textX = layout.field.x + inset * (panel.secret() ? 1.0f : 2.0f) +
                        (panel.secret() ? 0.0f : iconDp * dp);
    const float room = layout.field.right() - inset - textX;
    const std::string shown = panel.secret() ? dotsFor(panel.text()) : panel.text();
    if (shown.empty()) {
        drawFitted(typeface_, panel.prompt(), textX, layout.field.centreY(), room, field,
                   withAlpha(palette::inkSoft, alpha));
    } else {
        drawFitted(typeface_, shown, textX, layout.field.centreY(), room, field,
                   withAlpha(palette::ink, alpha));
    }
    if (std::fmod(seconds, caretPeriod) < caretPeriod * 0.5) {
        const float caretX =
            textX + (shown.empty() ? 0.0f : std::min(typeface_.measure(shown, field), room));
        const float caretHeight = typeface_.lineBox(field) * 0.8f;
        DrawRectangleRec(Rectangle{caretX + dp, layout.field.centreY() - caretHeight * 0.5f,
                                   1.6f * dp, caretHeight},
                         withAlpha(palette::ink, alpha));
    }

    // The results: as many rows as fit, from the first in view.
    const TextStyle result{resultSp * dp};
    const bool inResults = panel.zone() == SearchZone::Results;
    if (panel.lists() && panel.results().empty() && !panel.text().empty()) {
        typeface_.drawCentred("No results", layout.results[0].x + inset,
                              layout.results[0].centreY(), result,
                              withAlpha(palette::inkSoft, alpha));
    }
    for (std::size_t row = 0; row < layout.results.size(); ++row) {
        const std::size_t index = panel.firstListed() + row;
        if (!panel.lists() || index >= panel.results().size()) {
            break;
        }
        const Rect& box = layout.results[row];
        paintBar(box, inResults && index == panel.resultFocus(), dp, alpha);
        const SearchResult& entry = panel.results()[index];
        const float detail = typeface_.measure(entry.detail, result);
        typeface_.drawCentred(entry.detail, box.right() - inset - detail, box.centreY(), result,
                              withAlpha(palette::inkSoft, alpha));
        drawFitted(typeface_, entry.title, box.x + inset, box.centreY(),
                   box.width - detail - inset * 3.0f, result, withAlpha(palette::ink, alpha));
    }

    // The keys.
    const TextStyle cap{keySp * dp};
    const std::span<const SearchKey> keys = searchKeys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const bool focused = !inResults && i == panel.keyFocus();
        const bool done = keys[i].kind == KeyKind::Done;
        const RoundRect keyShape{layout.keys[i], cornerDp * dp};
        paintBar(layout.keys[i], focused && !done, dp, alpha);
        if (done) {
            fillRoundRect(keyShape, [alpha](Vector2, float) {
                return withAlpha(outlineInk, alpha);
            });
            if (focused) {
                fillBand(keyShape, keyShape.grown(-outlineDp * dp), [alpha](Vector2, float) {
                    return withAlpha(palette::ink, alpha);
                });
            }
        }
        const std::string label{keys[i].label};
        const float width = typeface_.measure(label, cap);
        typeface_.drawCentred(label, layout.keys[i].centreX() - width * 0.5f,
                              layout.keys[i].centreY(), cap,
                              withAlpha(done ? WHITE : static_cast<Color>(palette::ink), alpha));
    }

    // The buttons along the foot.
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float centreY = layout.hints.centreY();
    // A physical keyboard types straight into the field, so it needs no key prompts.
    const bool typing = prompts_->device.current() == input::Device::KeyboardMouse;
    std::vector<std::pair<const char*, const char*>> hints;
    if (typing) {
        hints = {{"A", inResults ? "Open" : "Done"}, {"B", "Close"}};
    } else {
        hints = {{"A", inResults ? "Open" : "Type"}, {"B", "Delete"}};
        if (panel.lists()) {
            hints.emplace_back("X", inResults ? "Keys" : "Results");
        } else if (panel.secret()) {
            hints.emplace_back("X", panel.shifted() ? "Caps on" : "Caps");
        }
        hints.insert(hints.end(), {{"Y", "Space"}, {"-", "Clear"}, {"+", "Done"}});
    }
    float total = 0.0f;
    for (const auto& [key, label] : hints) {
        total += glyphs_.advance(key, glyph) + hintGapDp * dp + typeface_.measure(label, hint) +
                 hintSpacingDp * dp;
    }
    float x = layout.hints.centreX() - (total - hintSpacingDp * dp) * 0.5f;
    for (const auto& [key, label] : hints) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(key, Vector2{x + advance * 0.5f, centreY}, glyph,
                      withAlpha(outlineInk, alpha));
        x += advance + hintGapDp * dp;
        typeface_.drawCentred(label, x, centreY, hint, withAlpha(outlineInk, alpha));
        x += typeface_.measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui

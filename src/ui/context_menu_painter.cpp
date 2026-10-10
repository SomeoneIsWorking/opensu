#include "context_menu_painter.hpp"

#include <array>
#include <string>
#include <utility>

#include "hud.hpp"
#include "panel_scope.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color cardFill{0xF8, 0xF8, 0xFB, 255};
constexpr Color rowFill{0xFF, 0xFF, 0xFF, 255};
constexpr Color outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float titleSp = 19.0f;
constexpr float rowSp = 17.0f;
constexpr float rowInsetDp = 16.0f;
constexpr float rowRadiusDp = 12.0f;
constexpr float outlineDp = 1.8f;
constexpr float hintSp = 13.0f;
constexpr float hintGlyphDp = 18.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 18.0f;

} // namespace

void ContextMenuPainter::paint(const ContextMenu& menu, Vector2 size, float dp,
                               const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, look.alpha));
    const ContextLayout layout = menu.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    const PanelScope scope{Vector2{layout.card.centreX(), layout.card.centreY()}, look.scale};
    fillRoundRect(RoundRect{layout.card, layout.radius}, [&](Vector2, float) {
        return withAlpha(cardFill, look.alpha);
    });

    const TextStyle title{titleSp * dp};
    const float room = layout.title.width;
    std::string name = menu.title();
    while (name.size() > 1 && typeface_.measure(name, title) > room) {
        // Back up over a whole UTF-8 character, then mark the cut.
        std::size_t cut = name.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0U) == 0x80U) {
            --cut;
        }
        name.resize(cut);
        name += "...";
        if (typeface_.measure(name, title) <= room) {
            break;
        }
        name.resize(name.size() - 3);
    }
    typeface_.drawCentred(name, layout.title.x, layout.title.centreY(), title,
                          withAlpha(palette::ink, look.alpha));

    const TextStyle row{rowSp * dp};
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const RoundRect bar{layout.rows[i], rowRadiusDp * dp};
        fillRoundRect(bar, [&](Vector2, float) {
            return withAlpha(rowFill, look.alpha);
        });
        typeface_.drawCentred(menu.items()[i].label, layout.rows[i].x + rowInsetDp * dp,
                              layout.rows[i].centreY(), row, withAlpha(palette::ink, look.alpha));
        if (i == menu.focus()) {
            fillBand(bar, bar.grown(-outlineDp * dp), [&](Vector2, float) {
                return withAlpha(outlineInk, look.alpha);
            });
        }
    }

    // The buttons along the foot, under the card.
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float centreY = layout.hints.centreY();
    float width = 0.0f;
    constexpr std::array<std::pair<const char*, const char*>, 2> hints{
        {{"A", "Select"}, {"B", "Close"}}};
    for (const auto& [key, label] : hints) {
        width += glyphs_.advance(key, glyph) + hintGapDp * dp + typeface_.measure(label, hint) +
                 hintSpacingDp * dp;
    }
    float x = layout.card.centreX() - (width - hintSpacingDp * dp) * 0.5f;
    for (const auto& [key, label] : hints) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(key, Vector2{x + advance * 0.5f, centreY}, glyph,
                      withAlpha(outlineInk, look.alpha));
        x += advance + hintGapDp * dp;
        typeface_.drawCentred(label, x, centreY, hint, withAlpha(outlineInk, look.alpha));
        x += typeface_.measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui

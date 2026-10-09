#include "game_menu_painter.hpp"

#include <algorithm>

#include "hud.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// The game shows through, dimmed, as under Steam's Guide menu.
constexpr Color scrim{0, 0, 0, 115};
constexpr Color panelFill{palette::ground.r, palette::ground.g, palette::ground.b, 245};
constexpr Color selection{0x4d, 0x46, 0x55, 255};
constexpr Color hintInk{0x4d, 0x46, 0x55, 255};

constexpr float panelWidthDp = 300.0f;
constexpr float paddingDp = 24.0f;
constexpr float titleSp = 22.0f;
constexpr float itemSp = 18.0f;
constexpr float itemHeightDp = 52.0f;
constexpr float itemGapDp = 6.0f;
constexpr float itemRadiusDp = 14.0f;
constexpr float hintSp = 14.0f;
constexpr float hintGlyphDp = 20.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 20.0f;

} // namespace

void GameMenuPainter::paint(const GameMenu& menu, float width, float height, float dp) const {
    if (!menu.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, width, height}, scrim);
    const float panel = std::min(panelWidthDp * dp, width);
    DrawRectangleRec(Rectangle{0.0f, 0.0f, panel, height}, panelFill);

    const float pad = paddingDp * dp;
    const TextStyle title{titleSp * dp};
    float y = pad + type().lineBox(title) * 0.5f;
    type().drawCentred(menu.title(), pad, y, title, palette::ink);
    y += type().lineBox(title) * 0.5f + pad;

    const TextStyle item{itemSp * dp};
    const float rowHeight = itemHeightDp * dp;
    const float inset = pad * 0.5f;
    for (std::size_t i = 0; i < menu.items().size(); ++i) {
        const Rectangle row{inset, y, panel - 2.0f * inset, rowHeight};
        const bool focused = i == menu.focus();
        if (focused) {
            DrawRectangleRounded(row, std::min(1.0f, 2.0f * itemRadiusDp * dp / rowHeight), 12,
                                 selection);
        }
        type().drawCentred(menu.items()[i].label, pad, y + rowHeight * 0.5f, item,
                           focused ? palette::panel : palette::ink);
        y += rowHeight + itemGapDp * dp;
    }

    // Button hints along the bottom: A selects, B resumes.
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float centreY = height - pad - glyph * 0.5f;
    float x = pad;
    for (const auto& [key, label] : {std::pair{"A", "Select"}, std::pair{"B", "Resume"}}) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(key, Vector2{x + advance * 0.5f, centreY}, glyph, hintInk);
        x += advance + hintGapDp * dp;
        type().drawCentred(label, x, centreY, hint, hintInk);
        x += type().measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui

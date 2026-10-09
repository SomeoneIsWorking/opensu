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

constexpr float titleSp = 22.0f;
constexpr float itemSp = 18.0f;
constexpr float itemRadiusDp = 14.0f;
constexpr float hintSp = 14.0f;
constexpr float hintGlyphDp = 20.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 20.0f;

} // namespace

GameMenuLayout GameMenuPainter::layout(float width, float height, float dp) const {
    return layoutGameMenu(width, height, dp, type().lineBox(TextStyle{titleSp * dp}));
}

void GameMenuPainter::paint(const GameMenu& menu, float width, float height, float dp) const {
    if (!menu.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, width, height}, scrim);
    const GameMenuLayout frame = layout(width, height, dp);
    DrawRectangleRec(Rectangle{frame.panel.x, frame.panel.y, frame.panel.width, frame.panel.height},
                     panelFill);

    const float pad = frame.padding;
    const TextStyle title{titleSp * dp};
    type().drawCentred(menu.title(), pad, pad + type().lineBox(title) * 0.5f, title, palette::ink);

    const TextStyle item{itemSp * dp};
    for (std::size_t i = 0; i < menu.items().size(); ++i) {
        const Rect& box = frame.rows[i];
        const Rectangle row{box.x, box.y, box.width, box.height};
        const bool focused = i == menu.focus();
        if (focused) {
            DrawRectangleRounded(row, std::min(1.0f, 2.0f * itemRadiusDp * dp / box.height), 12,
                                 selection);
        }
        type().drawCentred(menu.items()[i].label, pad, box.centreY(), item,
                           focused ? palette::panel : palette::ink);
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

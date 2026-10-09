#include "guide_menu_painter.hpp"

#include <algorithm>

#include "hud.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

// What is under the menu shows through, dimmed, as under Steam's Guide menu.
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
constexpr float hintLineGapDp = 4.0f;
// Degrees clockwise from 3 o'clock; the ring leaves a gap at 12 o'clock.
constexpr float powerRingStart = -55.0f;
constexpr float powerRingEnd = 235.0f;

} // namespace

GuideLayout GuideMenuPainter::layout(const GuideMenu& menu, float width, float height,
                                     float dp) const {
    return layoutGuide(PanelFrame{width, height, dp},
                       PanelChrome{type().lineBox(TextStyle{titleSp * dp}),
                                   (2.0f * hintGlyphDp + hintLineGapDp) * dp},
                       menu.entries().size());
}

void GuideMenuPainter::paintPowerButton(const Rect& box, bool focused, float dp) const {
    const Rectangle bounds{box.x, box.y, box.width, box.height};
    // Focus is the rows' rounded selection, so the button reads as one of them.
    if (focused) {
        DrawRectangleRounded(bounds, std::min(1.0f, 2.0f * itemRadiusDp * dp / box.height), 12,
                             selection);
    }
    const Color ink = focused ? palette::panel : palette::ink;
    const Vector2 centre{box.centreX(), box.centreY()};
    // About the rows' cap height, with their text's stroke weight.
    const float radius = box.width * 0.22f;
    const float stroke = std::max(1.75f * dp, 1.0f);
    // The standard power symbol: a ring open at the top with a bar through the gap.
    DrawRing(centre, radius - stroke * 0.5f, radius + stroke * 0.5f, powerRingStart, powerRingEnd,
             36, ink);
    const Vector2 top{centre.x, centre.y - radius * 1.1f};
    const Vector2 mid{centre.x, centre.y - radius * 0.1f};
    DrawLineEx(top, mid, stroke, ink);
    DrawCircleV(top, stroke * 0.5f, ink);
    DrawCircleV(mid, stroke * 0.5f, ink);
}

void GuideMenuPainter::paint(const GuideMenu& menu, float width, float height, float dp) const {
    if (!menu.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, width, height}, scrim);
    const GuideLayout frame = layout(menu, width, height, dp);
    DrawRectangleRec(Rectangle{frame.panel.x, frame.panel.y, frame.panel.width, frame.panel.height},
                     panelFill);

    const float pad = frame.padding;
    const TextStyle title{titleSp * dp};
    const float room = frame.panel.width - 2.0f * pad;
    type().drawCentred(type().fitted(menu.title(), room, title), pad,
                       pad + type().lineBox(title) * 0.5f, title, palette::ink);

    const TextStyle item{itemSp * dp};
    for (std::size_t i = 0; i < menu.entries().size() && i < frame.rows.size(); ++i) {
        const Rect& box = frame.rows[i];
        const Rectangle row{box.x, box.y, box.width, box.height};
        const bool focused = i == menu.focus() && !menu.powerFocused();
        if (focused) {
            DrawRectangleRounded(row, std::min(1.0f, 2.0f * itemRadiusDp * dp / box.height), 12,
                                 selection);
        }
        type().drawCentred(type().fitted(menu.shown(i), room, item), pad, box.centreY(), item,
                           focused ? palette::panel : palette::ink);
    }

    if (!menu.inPower()) {
        paintPowerButton(frame.powerButton, menu.powerFocused(), dp);
    }

    // The hints stack at the footer's right end: A selects over B going back.
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float right = frame.footer.right() - frame.padding * 0.5f;
    float centreY = frame.footer.centreY() - (glyph + hintLineGapDp * dp) * 0.5f;
    for (const auto& [key, label] : {std::pair{"A", "Select"}, std::pair{"B", menu.backLabel()}}) {
        const float labelWidth = type().measure(label, hint);
        const float advance = glyphs_.advance(key, glyph);
        const float x = right - labelWidth - hintGapDp * dp - advance;
        glyphs_.paint(key, Vector2{x + advance * 0.5f, centreY}, glyph, hintInk);
        type().drawCentred(label, right - labelWidth, centreY, hint, hintInk);
        centreY += glyph + hintLineGapDp * dp;
    }
}

} // namespace opensu::ui

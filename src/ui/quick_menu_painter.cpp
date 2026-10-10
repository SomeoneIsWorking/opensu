#include "quick_menu_painter.hpp"

#include <utility>

#include "clip_stack.hpp"
#include "hud.hpp"
#include "settings_page_painter.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color panelFill{palette::ground.r, palette::ground.g, palette::ground.b, 245};
constexpr Color hintInk{0x4d, 0x46, 0x55, 255};

constexpr float headingSp = 22.0f;
constexpr float hintSp = 14.0f;
constexpr float hintGlyphDp = 20.0f;
constexpr float hintGapDp = 6.0f;
constexpr float hintSpacingDp = 20.0f;

} // namespace

QuickLayout QuickMenuPainter::layout(const QuickMenu& menu, float width, float height,
                                     float dp) const {
    const float footer = hintGlyphDp * dp;
    return layoutQuick(PanelFrame{width, height, dp},
                       PanelChrome{typeface_.lineBox(TextStyle{headingSp * dp}), footer},
                       menu.rows(), menu.focus());
}

void QuickMenuPainter::paint(const QuickMenu& menu, float width, float height, float dp) const {
    if (!menu.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, width, height}, scrim);
    const QuickLayout frame = layout(menu, width, height, dp);
    DrawRectangleRec(Rectangle{frame.panel.x, 0.0f, frame.panel.width, height}, panelFill);

    const TextStyle heading{headingSp * dp};
    typeface_.drawCentred("Quick menu", frame.heading.x, frame.heading.centreY(), heading,
                          palette::ink);

    {
        ClipStack clips;
        const ScopedClip content{clips, frame.content};
        for (std::size_t i = 0; i < menu.rows().size() && i < frame.rows.size(); ++i) {
            const SettingsRowBox& box = frame.rows[i];
            if (box.rect.bottom() < frame.content.y || box.rect.y > frame.content.bottom()) {
                continue;
            }
            rows_.paintRow(menu.rows()[i], box, i == menu.focus(), dp, 1.0f);
        }
    }

    // Button hints along the bottom: A changes or selects, B closes.
    const SettingsRow* focused = menu.focusedRow();
    const bool changes = focused != nullptr && focused->kind != RowKind::Info;
    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const float centreY = height - frame.padding - glyph * 0.5f;
    float x = frame.heading.x;
    for (const auto& [key, label] :
         {std::pair{"A", changes ? "Change" : "Select"}, std::pair{"B", "Close"}}) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(key, Vector2{x + advance * 0.5f, centreY}, glyph, hintInk);
        x += advance + hintGapDp * dp;
        typeface_.drawCentred(label, x, centreY, hint, hintInk);
        x += typeface_.measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui

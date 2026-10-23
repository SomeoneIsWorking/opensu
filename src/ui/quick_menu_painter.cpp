#include "quick_menu_painter.hpp"

namespace opensu::ui {

QuickLayout QuickMenuPainter::layout(const QuickMenu& menu, float width, float height,
                                     float dp) const {
    return layoutQuick(PanelFrame{width, height, dp},
                       PanelChrome{chrome_.headingBox(dp), SidePanelPainter::footer(dp)},
                       menu.rows(), menu.focus());
}

void QuickMenuPainter::paint(Canvas& canvas, const QuickMenu& menu, float width, float height,
                             float dp) const {
    if (!menu.isOpen()) {
        return;
    }
    const QuickLayout frame = layout(menu, width, height, dp);
    chrome_.paintGround(canvas, frame.panel, width, height);
    chrome_.paintHeading(canvas, "Quick menu", frame.heading, dp);
    {
        const ScopedClip content{canvas, frame.content};
        for (std::size_t i = 0; i < menu.rows().size() && i < frame.rows.size(); ++i) {
            const SettingsRowBox& box = frame.rows[i];
            if (box.rect.bottom() < frame.content.y || box.rect.y > frame.content.bottom()) {
                continue;
            }
            rows_.paintRow(canvas, menu.rows()[i], box, i == menu.focus(), dp, 1.0f,
                           menu.editing() && i == menu.focus());
        }
    }
    // A changes a switch outright or the row A opened; B closes.
    const SettingsRow* focused = menu.focusedRow();
    const bool changes = menu.editing() || (focused != nullptr && focused->kind == RowKind::Toggle);
    chrome_.paintHints(canvas, {{"A", changes ? "Change" : "Select"}, {"B", "Close"}},
                       frame.heading.x, height, frame.padding, dp);
}

} // namespace opensu::ui

#include "settings_page_painter.hpp"

#include <algorithm>
#include <string>

#include "clip_stack.hpp"
#include "hud.hpp"
#include "round_shape.hpp"
#include "row_controls.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Color cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Color chosenFill{0xE6, 0xE4, 0xEE, 255};
constexpr float categorySp = 17.0f;
constexpr float labelSp = 17.0f;
constexpr float noteSp = 12.5f;
constexpr float valueSp = 15.5f;
constexpr float insetDp = 16.0f;
constexpr float outlineDp = 1.8f;
constexpr float rowRadiusDp = 12.0f;
constexpr float valueRoomShare = 0.5f;

Color faded(Color colour, float alpha) {
    return withAlpha(colour, alpha);
}

void paintCategories(const SettingsPage& page, const SettingsLayout& layout, float dp,
                     float alpha) {
    const TextStyle text{categorySp * dp};
    const bool inList = page.zone() == SettingsZone::Categories;
    for (std::size_t i = 0; i < layout.categoryCells.size(); ++i) {
        const Rect& cell = layout.categoryCells[i];
        const RoundRect body{cell, layout.cellRadius};
        const bool chosen = i == page.category();
        fillRoundRect(body, [chosen, alpha](Vector2, float) {
            return faded(chosen ? chosenFill : cardFill, alpha * (chosen ? 1.0f : 0.92f));
        });
        if (chosen && inList) {
            fillBand(body, body.grown(-outlineDp * dp), [alpha](Vector2, float) {
                return faded(rowInk, alpha);
            });
        }
        type().drawCentred(page.categories()[i].label, cell.x + insetDp * dp, cell.centreY(), text,
                           faded(palette::ink, alpha));
    }
}

/// What a row shows at its end, right-aligned inside `rect`; the room it needs is returned.
void paintEnd(const SettingsRow& row, const SettingsRowBox& box, float dp, float alpha) {
    const TextStyle value{valueSp * dp};
    const float endX = box.rect.right() - insetDp * dp;
    const float room = box.rect.width * valueRoomShare;
    switch (row.kind) {
    case RowKind::Toggle:
        paintSwitch(box.control, row.on, dp);
        break;
    case RowKind::Slider:
        paintSlider(box.control, row.level, row.low, row.high, dp);
        break;
    case RowKind::Choice: {
        const std::string shown = type().fitted("<  " + row.value + "  >", room, value);
        type().drawCentred(shown, endX - type().measure(shown, value), box.rect.centreY(), value,
                           faded(palette::inkSoft, alpha));
        break;
    }
    case RowKind::Folder:
    case RowKind::Action: {
        const std::string shown = type().fitted(row.value, room - chevronWidthDp * dp, value);
        type().drawCentred(shown, endX - chevronWidthDp * dp - type().measure(shown, value),
                           box.rect.centreY(), value, faded(palette::inkSoft, alpha));
        paintChevron(endX, box.rect.centreY(), dp);
        break;
    }
    case RowKind::Info: {
        const std::string shown = type().fitted(row.value, room, value);
        type().drawCentred(shown, endX - type().measure(shown, value), box.rect.centreY(), value,
                           faded(palette::inkSoft, alpha));
        break;
    }
    }
}

} // namespace

void SettingsPagePainter::paintRow(const SettingsRow& row, const SettingsRowBox& box,
                                   bool focused, float dp, float alpha) {
    const RoundRect bar{box.rect, rowRadiusDp * dp};
    fillRoundRect(bar, [alpha](Vector2, float) {
        return faded(cardFill, alpha);
    });
    const TextStyle label{labelSp * dp};
    const TextStyle note{noteSp * dp};
    const float x = box.rect.x + insetDp * dp;
    float room = box.rect.width * (1.0f - valueRoomShare) - insetDp * dp;
    if (box.control.width > 0.0f) {
        // A toggle or a slider's track starts where the text must end.
        room = std::min(room, box.control.x - x - insetDp * 0.5f * dp);
    }
    if (row.note.empty()) {
        type().drawCentred(type().fitted(row.label, room, label), x, box.rect.centreY(), label,
                           faded(palette::ink, alpha));
    } else {
        const float shift = type().lineBox(note) * 0.5f + 1.0f * dp;
        type().drawCentred(type().fitted(row.label, room, label), x, box.rect.centreY() - shift,
                           label, faded(palette::ink, alpha));
        type().drawCentred(type().fitted(row.note, room, note), x,
                           box.rect.centreY() + shift * 1.4f, note, faded(palette::inkSoft, alpha));
    }
    paintEnd(row, box, dp, alpha);
    if (focused) {
        fillBand(bar, bar.grown(-outlineDp * dp), [alpha](Vector2, float) {
            return faded(rowInk, alpha);
        });
    }
}

void SettingsPagePainter::paint(const SettingsPage& page, const SettingsLayout& layout,
                                Vector2 size, float dp, float alpha) {
    if (alpha <= 0.0f) {
        return;
    }
    DrawRectangle(0, 0, static_cast<int>(size.x), static_cast<int>(size.y),
                  faded(palette::ground, alpha));
    paintCategories(page, layout, dp, alpha);
    fillRoundRect(RoundRect{layout.panel, layout.radius}, [alpha](Vector2, float) {
        return faded(panelFill, alpha);
    });
    ClipStack clips;
    const ScopedClip content{clips, layout.panel};
    const std::vector<SettingsRow>& rows = page.focusedCategory().rows;
    for (std::size_t i = 0; i < rows.size() && i < layout.rows.size(); ++i) {
        const SettingsRowBox& box = layout.rows[i];
        if (box.rect.bottom() < layout.panel.y || box.rect.y > layout.panel.bottom()) {
            continue;
        }
        SettingsPagePainter::paintRow(rows[i], box, page.zone() == SettingsZone::Rows && i == page.row(), dp, alpha);
    }
}

} // namespace opensu::ui

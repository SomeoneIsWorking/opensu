#include "settings_tabs_painter.hpp"

#include "hud.hpp"
#include "round_shape.hpp"
#include "row_controls.hpp"

namespace opensu::ui {
namespace {

constexpr float tabSp = 14.5f;
constexpr float tabInsetDp = 8.0f;
constexpr float shoulderGlyphDp = 22.0f;

} // namespace

TabsLayout SettingsTabsPainter::layout(const SettingsTabs& page, float width, float height,
                                       float dp) const {
    const TextStyle text{tabSp * dp};
    std::vector<float> labels;
    for (const std::string& tab : page.view().tabs) {
        labels.push_back(typeface_.measure(tab, text));
    }
    return layoutTabs(PanelFrame{width, height, dp},
                      PanelChrome{chrome_.headingBox(dp), SidePanelPainter::footer(dp)},
                      page.view(), page.focus(), labels);
}

void SettingsTabsPainter::paintStrip(Canvas& canvas, const SettingsTabs& page,
                                     const TabsLayout& frame, float dp) const {
    const TextStyle text{tabSp * dp};
    const TabsView& view = page.view();
    for (std::size_t i = 0; i < frame.tabs.size() && i < view.tabs.size(); ++i) {
        const Rect& cell = frame.tabs[i];
        const bool shown = i == view.tab;
        fillRoundRect(canvas, RoundRect{cell, cell.height * 0.5f}, [shown](Point, float) {
            return shown ? rowInk : rowChosen;
        });
        const std::string label =
            typeface_.fitted(view.tabs[i], cell.width - 2.0f * tabInsetDp * dp, text);
        typeface_.drawCentred(canvas, label, cell.centreX() - typeface_.measure(label, text) * 0.5f,
                              cell.centreY(), text, shown ? white : palette::ink);
    }
    const float glyph = shoulderGlyphDp * dp;
    const float y = frame.strip.centreY();
    chrome_.paintGlyph(canvas, "LB", Point{frame.strip.x + glyph * 0.6f, y}, glyph, rowInk);
    chrome_.paintGlyph(canvas, "RB", Point{frame.strip.right() - glyph * 0.6f, y}, glyph, rowInk);
}

void SettingsTabsPainter::paint(Canvas& canvas, const SettingsTabs& page, float width, float height,
                                float dp) const {
    if (!page.isOpen()) {
        return;
    }
    const TabsLayout frame = layout(page, width, height, dp);
    chrome_.paintGround(canvas, frame.panel, width, height);
    chrome_.paintHeading(canvas, page.view().title, frame.heading, dp);
    paintStrip(canvas, page, frame, dp);
    {
        const ScopedClip content{canvas, frame.content};
        const std::vector<TabRow>& rows = page.view().rows;
        for (std::size_t i = 0; i < rows.size() && i < frame.rows.size(); ++i) {
            const Rect& rect = frame.rows[i];
            if (rect.bottom() < frame.content.y || rect.y > frame.content.bottom()) {
                continue;
            }
            const bool focused = i == page.focus();
            if (rows[i].row.kind == RowKind::Action) {
                rows_.paintTabRow(
                    canvas, rows[i],
                    SettingsPagePainter::TabRowBox{rect, frame.checkboxes[i], frame.values[i]},
                    focused, page.column(), dp);
            } else {
                rows_.paintRow(canvas, rows[i].row, settingsRowBox(rect, rows[i].row.kind, dp),
                               focused, dp, 1.0f);
            }
        }
    }
    const TabRow* focused = page.focusedRow();
    std::string press = "Open";
    if (focused == nullptr || focused->row.kind == RowKind::Info) {
        press = "Select";
    } else if (page.column() == TabColumn::Override) {
        press = focused->overridden ? "Inherit" : "Override";
    }
    chrome_.paintHints(canvas, {{"A", press}, {"B", "Back"}}, frame.heading.x, height,
                       frame.padding, dp);
}

} // namespace opensu::ui

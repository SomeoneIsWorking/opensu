// The quick menu's rows, focus and geometry.
#include "quick_menu.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::QuickLayout;
using opensu::ui::QuickMenu;
using opensu::ui::RowKind;
using opensu::ui::SettingsRow;

SettingsRow row(const char* id, RowKind kind) {
    SettingsRow made;
    made.id = id;
    made.kind = kind;
    made.label = id;
    return made;
}

std::vector<SettingsRow> rows() {
    SettingsRow volume = row("volume", RowKind::Slider);
    volume.level = 40;
    volume.high = 100;
    return {volume, row("mute", RowKind::Toggle), row("output", RowKind::Choice),
            row("close", RowKind::Action)};
}

void focusMovesAndStops() {
    QuickMenu menu;
    expect(!menu.isOpen() && menu.focusedRow() == nullptr, "closed with no rows");
    menu.open(rows());
    expect(menu.isOpen() && menu.focusedRow()->id == "volume", "opens on the first row");
    expect(!menu.move(-1) && menu.move(1) && menu.focusedRow()->id == "mute", "moves down");
    expect(menu.move(9) && menu.focus() == 3 && !menu.move(1), "stops at the end");
    expect(menu.focusRow(0) && !menu.focusRow(0) && !menu.focusRow(9),
           "a pointer focuses a row once, and not one that is not there");
}

void refreshKeepsTheFocusedRow() {
    QuickMenu menu;
    menu.open(rows());
    menu.focusRow(2);
    std::vector<SettingsRow> changed = rows();
    changed.insert(changed.begin(), row("brightness", RowKind::Slider));
    menu.refresh(changed);
    expect(menu.focusedRow()->id == "output", "the focus follows the row, not the index");
    menu.refresh({row("volume", RowKind::Slider)});
    expect(menu.focusedRow()->id == "volume", "a row that went leaves the focus on the last one");
    menu.close();
    menu.refresh(rows());
    expect(menu.rows().size() == 1, "a closed menu is not refreshed");
}

void layoutIsOnTheRight() {
    const std::vector<SettingsRow> shown = rows();
    const QuickLayout layout =
        opensu::ui::layoutQuick({1920.0f, 1080.0f, 2.0f}, {40.0f, 40.0f}, shown, 0);
    expect(layout.panel.right() == 1920.0f && layout.panel.width == 760.0f,
           "the panel is 380 dp wide down the right edge");
    expect(layout.rows.size() == shown.size(), "a box per row");
    const auto hit = layout.rowAt(layout.rows[1].rect.centreX(), layout.rows[1].rect.centreY());
    expect(hit && *hit == 1, "a row's centre hits it");
    expect(!layout.rowAt(100.0f, 100.0f), "the screen left of the panel hits none");
    const opensu::ui::SettingsRowBox& track = layout.rows[0];
    const auto level = layout.levelAt(shown[0], 0, track.control.x + track.control.width * 0.5f,
                                      track.control.y + 2.0f);
    expect(level && *level >= 49 && *level <= 51, "the middle of a slider's track is half way");
    expect(
        !layout.levelAt(shown[1], 1, layout.rows[1].rect.centreX(), layout.rows[1].rect.centreY()),
        "a switch has no level");
}

void focusedRowScrollsIntoView() {
    const std::vector<SettingsRow> many(30, row("r", RowKind::Action));
    const QuickLayout top = opensu::ui::layoutQuick({1280.0f, 600.0f, 1.0f}, {30.0f, 30.0f}, many, 0);
    const QuickLayout end = opensu::ui::layoutQuick({1280.0f, 600.0f, 1.0f}, {30.0f, 30.0f}, many, 29);
    expect(end.rows[29].rect.bottom() <= end.content.bottom() + 0.5f,
           "the last row is scrolled whole into view");
    expect(end.rows[0].rect.y < top.rows[0].rect.y, "the rows moved up to show it");
}

} // namespace

int main() {
    focusMovesAndStops();
    refreshKeepsTheFocusedRow();
    layoutIsOnTheRight();
    focusedRowScrollsIntoView();
    std::printf("quick_menu: all checks passed\n");
    return 0;
}

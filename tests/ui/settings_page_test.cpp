// The Settings screen's model: focus across the category list and the rows, refresh by id, and the
// geometry the pointer and painter share.
#include "settings_page.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;

SettingsRow row(const char* id, RowKind kind) {
    SettingsRow made;
    made.id = id;
    made.kind = kind;
    made.label = id;
    return made;
}

std::vector<SettingsCategory> categories() {
    return {{"a", "Alpha", {row("a1", RowKind::Toggle), row("a2", RowKind::Choice)}},
            {"b", "Beta", {}},
            {"c",
             "Gamma",
             {row("c1", RowKind::Slider), row("c2", RowKind::Folder), row("c3", RowKind::Info)}}};
}

void focusMovesAcrossBothZones() {
    SettingsPage page;
    page.open(categories());
    expect(page.isOpen() && page.zone() == SettingsZone::Categories && page.category() == 0,
           "opens on the first category in the list");
    expect(page.focusedRow() == nullptr, "no row is focused in the list");
    expect(page.move(1) && page.category() == 1 && !page.enterRows(),
           "a category without rows is not entered");
    expect(page.move(1) && page.category() == 2 && !page.move(1), "the list stops at its end");
    expect(page.enterRows() && page.zone() == SettingsZone::Rows && page.row() == 0,
           "enter the rows");
    expect(page.move(5) && page.row() == 2 && !page.move(1), "rows stop at the end");
    expect(page.focusedRow()->id == "c3", "the focused row");
    expect(page.leaveRows() && !page.leaveRows() && page.zone() == SettingsZone::Categories,
           "leave once");
}

void focusByPointer() {
    SettingsPage page;
    page.open(categories());
    expect(page.focusCategory(2) && !page.focusCategory(2), "a category is focused once");
    expect(!page.focusCategory(9), "out of range is refused");
    expect(page.focusRow(1) && page.zone() == SettingsZone::Rows && page.row() == 1,
           "a row enters the rows");
    expect(!page.focusRow(7), "no such row");
    expect(page.focusCategory(0) && page.zone() == SettingsZone::Categories && page.row() == 0,
           "a category leaves the rows and resets the row");
}

void refreshKeepsFocusById() {
    SettingsPage page;
    page.open(categories());
    page.focusCategory(2);
    page.focusRow(1);
    std::vector<SettingsCategory> next = categories();
    next[2].rows.insert(next[2].rows.begin(), row("new", RowKind::Info));
    page.refresh(next);
    expect(page.category() == 2 && page.focusedRow()->id == "c2" &&
               page.zone() == SettingsZone::Rows,
           "the focused row is followed by id");
    next[2].rows.erase(next[2].rows.begin() + 2);
    page.refresh(next);
    expect(page.row() < page.focusedCategory().rows.size(), "a vanished row leaves focus in range");
    page.refresh({{"z", "Zed", {}}});
    expect(page.category() == 0 && page.zone() == SettingsZone::Categories,
           "a vanished category resets");
}

void layoutFitsAndScrolls() {
    std::vector<SettingsRow> many;
    many.reserve(30);
    for (int i = 0; i < 30; ++i) {
        many.push_back(row("r", i == 3 ? RowKind::Slider : RowKind::Toggle));
    }
    const SettingsLayout top = layoutSettings(1920, 1080, 2.25f, 100, 100, 5, many, 0);
    const SettingsLayout bottom = layoutSettings(1920, 1080, 2.25f, 100, 100, 5, many, 29);
    expect(top.categoryCells.size() == 5 && top.rows.size() == 30, "a cell and a box each");
    expect(top.panel.y >= 100 && top.panel.bottom() <= 1080 - 100 + 0.5f, "inside the insets");
    expect(bottom.rows[29].rect.y >= bottom.panel.y &&
               bottom.rows[29].rect.bottom() <= bottom.panel.bottom() + 0.5f,
           "the focused last row is whole in the panel");
    expect(bottom.rows[0].rect.y < top.rows[0].rect.y, "the rows above scrolled up");
    expect(!bottom.rowAt(bottom.panel.centreX(), bottom.rows[0].rect.centreY()),
           "a scrolled-off row is not hit");
    const auto hit = top.rowAt(top.rows[1].rect.centreX(), top.rows[1].rect.centreY());
    expect(hit && *hit == 1, "a row's centre hits it");
    expect(top.categoryAt(top.categoryCells[2].centreX(), top.categoryCells[2].centreY()) == 2,
           "a category cell's centre hits it");
    expect(!top.categoryAt(top.panel.centreX(), top.panel.centreY()), "the panel is no category");
}

void categoriesShrinkToFit() {
    const SettingsLayout roomy = layoutSettings(1920, 1080, 1.5f, 100, 100, 6, {}, 0);
    const SettingsLayout cramped = layoutSettings(1280, 800, 2.25f, 130, 130, 6, {}, 0);
    expect(roomy.categoryCells[0].height > cramped.categoryCells[0].height / 2.25f * 1.5f - 0.1f,
           "with room the cells keep their size");
    expect(cramped.categoryCells[5].bottom() <= 800 - 130 + 0.5f,
           "a list that would run past the page is squeezed into it");
    expect(cramped.categoryCells[0].height >= 44.0f * 2.25f * 0.5f - 0.1f, "but never below half");
    const SettingsLayout absurd = layoutSettings(1280, 800, 2.25f, 300, 300, 6, {}, 0);
    expect(absurd.categoryCells[0].height >= 44.0f * 2.25f * 0.5f - 0.1f,
           "even when there is no room");
}

void sliderTrackLeavesTheLabelRoom() {
    SettingsRow slider = row("s", RowKind::Slider);
    slider.low = 0;
    slider.high = 10;
    const SettingsLayout narrow = layoutSettings(1280, 800, 2.25f, 130, 130, 1, {slider}, 0);
    const SettingsRowBox& box = narrow.rows[0];
    expect(box.control.width <= box.rect.width * 0.4f + 0.1f,
           "the track is at most two fifths of the row");
    expect(box.control.x > box.rect.x + box.rect.width * 0.35f,
           "so the label keeps over a third of it");
}

void sliderMapsToLevels() {
    SettingsRow slider = row("s", RowKind::Slider);
    slider.low = 1;
    slider.high = 20;
    std::vector<SettingsRow> rows{slider};
    const SettingsLayout layout = layoutSettings(1920, 1080, 2.25f, 100, 100, 1, rows, 0);
    const Rect& track = layout.rows[0].control;
    const float y = track.centreY();
    expect(layout.levelAt(slider, 0, track.x, y) == 1, "the start is the low level");
    expect(layout.levelAt(slider, 0, track.right() - 0.5f, y) == 20, "the end is the high level");
    expect(!layout.levelAt(slider, 0, track.x - 4, y), "left of the track is none");
    SettingsRow toggle = row("t", RowKind::Toggle);
    expect(!layout.levelAt(toggle, 0, track.centreX(), y), "only a slider has levels");
}

} // namespace

int main() {
    focusMovesAcrossBothZones();
    focusByPointer();
    refreshKeepsFocusById();
    layoutFitsAndScrolls();
    categoriesShrinkToFit();
    sliderTrackLeavesTheLabelRoom();
    sliderMapsToLevels();
    std::printf("settings_page: all checks passed\n");
    return 0;
}

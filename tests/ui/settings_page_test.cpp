// The Settings screen's model: focus across the category list and the rows, refresh by id, and the
// geometry the pointer and painter share.
#include "settings_page.hpp"

#include <cstdio>

#include "check.hpp"
#include "row_activation.hpp"

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
    const SettingsLayout top = layoutSettings(1920, 1080, 2.25f, 100, 100, 5, 0, many, 0);
    const SettingsLayout bottom = layoutSettings(1920, 1080, 2.25f, 100, 100, 5, 0, many, 29);
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

void categoriesScrollIntoView() {
    const SettingsLayout top = layoutSettings(1920, 1080, 2.25f, 100, 100, 40, 0, {}, 0);
    const SettingsLayout bottom = layoutSettings(1920, 1080, 2.25f, 100, 100, 40, 39, {}, 0);
    expect(top.categoryCells[0].height == bottom.categoryCells[0].height,
           "the category cells keep their size");
    expect(bottom.categoryCells[39].bottom() <= 1080.0f - 100.0f + 0.5f,
           "the focused last one is whole inside the page");
    expect(bottom.categoryCells[0].y < top.categoryCells[0].y, "the list scrolled up");
    expect(!bottom.categoryAt(bottom.categoryCells[0].centreX(), bottom.categoryCells[0].centreY()),
           "a scrolled-off category is not hit");
    expect(top.categoryAt(top.categoryCells[0].centreX(), top.categoryCells[0].centreY()) == 0,
           "the first is hit at the top");
}

void onlyAChoiceOrSliderOpens() {
    SettingsPage page;
    page.open(categories());
    expect(page.focusRow(0) && !page.beginEdit(), "a toggle is not opened for Left and Right");
    expect(page.focusRow(1) && page.beginEdit() && page.editing(), "a choice is");
    expect(page.move(-1) && !page.editing(), "moving off the row closes it");
    expect(page.focusCategory(2) && page.focusRow(0) && page.beginEdit() && page.editing(),
           "a slider is opened too");
    expect(page.leaveRows() && !page.editing(), "leaving the rows closes it");
    expect(page.focusRow(0) && page.beginEdit() && page.endEdit() && !page.endEdit(),
           "and it closes once");
}

void sliderTrackLeavesTheLabelRoom() {
    SettingsRow slider = row("s", RowKind::Slider);
    slider.low = 0;
    slider.high = 10;
    const SettingsLayout narrow = layoutSettings(1280, 800, 2.25f, 130, 130, 1, 0, {slider}, 0);
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
    const SettingsLayout layout = layoutSettings(1920, 1080, 2.25f, 100, 100, 1, 0, rows, 0);
    const Rect& track = layout.rows[0].control;
    const float y = track.centreY();
    expect(layout.levelAt(slider, 0, track.x, y) == 1, "the start is the low level");
    expect(layout.levelAt(slider, 0, track.right() - 0.5f, y) == 20, "the end is the high level");
    expect(!layout.levelAt(slider, 0, track.x - 4, y), "left of the track is none");
    SettingsRow toggle = row("t", RowKind::Toggle);
    expect(!layout.levelAt(toggle, 0, track.centreX(), y), "only a slider has levels");
}

void oneRuleForEveryRow() {
    using opensu::gamepad::Button;
    expect(rowOpens(RowKind::Choice) && rowOpens(RowKind::Slider), "a choice and a slider open");
    expect(!rowOpens(RowKind::Toggle) && !rowOpens(RowKind::Action) && !rowOpens(RowKind::Info) &&
               !rowOpens(RowKind::Folder),
           "nothing else does");
    RowEditing editing;
    expect(!editing.end() && editing.begin(RowKind::Choice) && editing.open() && editing.end() &&
               !editing.end(),
           "the open state begins and ends once");
    expect(!editing.begin(RowKind::Toggle), "a switch does not open");

    SettingsRow choice = row("c", RowKind::Choice);
    SettingsRow toggle = row("t", RowKind::Toggle);
    // In the categories, with no row focused: Right and A enter the rows.
    expect(rowEffect(Button::Right, nullptr, false, true).effect == RowEffect::Enter &&
               rowEffect(Button::A, nullptr, false, true).effect == RowEffect::Enter,
           "Right and A enter the rows");
    // In the rows, closed.
    expect(rowEffect(Button::Left, &choice, false, true).effect == RowEffect::Leave,
           "Left leaves them, whatever the row is");
    expect(rowEffect(Button::A, &choice, false, true).effect == RowEffect::Open,
           "A opens a choice");
    expect(rowEffect(Button::A, &toggle, false, true).effect == RowEffect::Act,
           "A acts on a switch");
    expect(rowEffect(Button::Right, &choice, false, true).effect == RowEffect::Nothing,
           "Right does nothing until A opens the row");
    // Open.
    const RowStep step = rowEffect(Button::Right, &choice, true, true);
    expect(step.effect == RowEffect::Change && step.step == 1, "Right changes it");
    expect(rowEffect(Button::Left, &choice, true, true).effect == RowEffect::Change &&
               rowEffect(Button::Left, &choice, true, true).step == -1,
           "Left changes it back");
    expect(rowEffect(Button::A, &choice, true, true).effect == RowEffect::Close &&
               rowEffect(Button::B, &choice, true, true).effect == RowEffect::Close,
           "A and B close it");
    // A screen with no categories has no rows to enter.
    expect(rowEffect(Button::Right, &choice, false, false).effect == RowEffect::Nothing &&
               rowEffect(Button::A, nullptr, false, false).effect == RowEffect::Nothing,
           "a screen without categories has nothing to enter");
}

} // namespace

int main() {
    focusMovesAcrossBothZones();
    focusByPointer();
    refreshKeepsFocusById();
    layoutFitsAndScrolls();
    categoriesScrollIntoView();
    onlyAChoiceOrSliderOpens();
    oneRuleForEveryRow();
    sliderTrackLeavesTheLabelRoom();
    sliderMapsToLevels();
    std::printf("settings_page: all checks passed\n");
    return 0;
}

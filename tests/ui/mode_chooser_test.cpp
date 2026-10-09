// The Library options panel: its rows, their geometry and scrolling, and its focus.
#include "mode_chooser.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::library::LibraryMode;
using opensu::test::expect;
using opensu::test::near;
using opensu::ui::ChooserLayout;
using opensu::ui::ChooserRow;
using opensu::ui::ChooserValues;
using opensu::ui::ModeChooser;
using opensu::ui::Rect;

constexpr float dp1080 = 2.25f;
const Rect frame1080{0.0f, 0.0f, 1920.0f, 1080.0f};
std::vector<ChooserRow> libraryRows() {
    return {ChooserRow::Cards,  ChooserRow::IconSize, ChooserRow::Pin,
            ChooserRow::Sort,   ChooserRow::Source,   ChooserRow::Installed,
            ChooserRow::Hidden, ChooserRow::Search,   ChooserRow::Settings};
}

ChooserLayout layoutAt(ChooserRow focused) {
    return opensu::ui::layoutChooser(frame1080, dp1080, libraryRows(), focused);
}

void cardsKeepIisusMeasures() {
    // navigation.md §5.2 on 1920 x 1080 at 2.25 px per dp: cards 352 px square, 24 px apart.
    const ChooserLayout chooser = layoutAt(ChooserRow::Cards);
    near(chooser.panel.width, 1178.1, "the panel is 523.6 dp wide", 0.1);
    near(chooser.panel.centreX(), 960.0, "centred", 0.01);
    near(chooser.cards[0].width, 352.3, "cards are 352 px", 0.5);
    near(chooser.cards[0].height, chooser.cards[0].width, "square", 0.01);
    near(chooser.cards[1].x - chooser.cards[0].right(), 24.1, "24 px apart", 0.1);
    near(chooser.cards[0].x - chooser.panel.x, 36.0, "36 px in from the side", 0.01);
    near(chooser.cards[0].y - chooser.panel.y, 47.25, "47 px down from the top", 0.01);
    near(chooser.panel.right() - chooser.cards[2].right(), 36.0, "and 36 px from the other side",
         0.01);
}

void cardsUnderThePointer() {
    const ChooserLayout chooser = layoutAt(ChooserRow::Cards);
    for (std::size_t i = 0; i < chooser.cards.size(); ++i) {
        const auto hit = chooser.cardAt(chooser.cards[i].centreX(), chooser.cards[i].centreY());
        expect(hit && *hit == opensu::library::allLibraryModes[i], "a card's centre hits it");
    }
    const Rect& left = chooser.cards[0];
    const Rect& middle = chooser.cards[1];
    expect(!chooser.cardAt((left.right() + middle.x) * 0.5f, left.centreY()),
           "the gap between cards hits none");
    expect(!chooser.cardAt(chooser.panel.x + 1.0f, chooser.panel.y + 1.0f),
           "the panel's padding hits none");
}

void panelStaysInTheFrame() {
    const ChooserLayout chooser = layoutAt(ChooserRow::Cards);
    expect(chooser.panel.y >= 0.0f && chooser.panel.bottom() <= 1080.0f,
           "the panel fits the frame when its rows are taller");
    near(chooser.panel.height, 1080.0 * 0.92, "and takes at most 92% of it", 0.01);
    near(chooser.panel.centreY(), 540.0, "centred vertically", 0.01);
}

void focusedRowScrollsIntoView() {
    const ChooserLayout top = layoutAt(ChooserRow::Cards);
    const ChooserLayout bottom = layoutAt(ChooserRow::Settings);
    const opensu::ui::ChooserRowBox* search = bottom.find(ChooserRow::Settings);
    expect(search != nullptr, "the settings row is there");
    expect(search->rect.y >= bottom.content.y && search->rect.bottom() <= bottom.content.bottom(),
           "the focused last row is whole inside the panel");
    expect(bottom.find(ChooserRow::Cards)->rect.y < top.find(ChooserRow::Cards)->rect.y,
           "the rows above it scrolled up");
    expect(bottom.cards[0].y < bottom.content.y, "the cards are partly scrolled off the top");
    expect(!bottom.cardAt(bottom.cards[0].centreX(), bottom.cards[0].y + 1.0f),
           "a card's scrolled-off part is not hit");
    const opensu::ui::ChooserRowBox* sort = top.find(ChooserRow::Sort);
    expect(sort != nullptr && sort->rect.bottom() <= top.content.bottom(),
           "with the cards focused the sort row is still in view");
}

void rowsUnderThePointer() {
    const ChooserLayout chooser = layoutAt(ChooserRow::IconSize);
    for (const opensu::ui::ChooserRowBox& box : chooser.rows) {
        if (box.row == ChooserRow::Cards || box.rect.bottom() > chooser.content.bottom() ||
            box.rect.y < chooser.content.y) {
            continue;
        }
        const auto hit = chooser.rowAt(box.rect.x + 2.0f, box.rect.centreY());
        expect(hit && *hit == box.row, "a row's edge hits it");
    }
    const opensu::ui::ChooserRowBox* pin = chooser.find(ChooserRow::Pin);
    expect(pin != nullptr && pin->rect.contains(pin->control.centreX(), pin->control.centreY()),
           "the switch is in its row");
    expect(!chooser.rowAt(chooser.panel.x - 5.0f, chooser.panel.centreY()),
           "outside the panel hits no row");
}

void sliderMapsPositionsToLevels() {
    const ChooserLayout chooser = layoutAt(ChooserRow::IconSize);
    const opensu::ui::ChooserRowBox* slider = chooser.find(ChooserRow::IconSize);
    expect(slider != nullptr, "the icon size row is there");
    const Rect& track = slider->control;
    const float y = track.centreY();
    expect(chooser.iconSizeAt(track.x, y) == 1, "the track's start is level 1");
    expect(chooser.iconSizeAt(track.right() - 0.5f, y) == 20, "its end is level 20");
    expect(chooser.iconSizeAt(track.x + track.width * (8.0f / 19.0f), y) == 9,
           "level 9 is 8/19 along");
    expect(!chooser.iconSizeAt(track.x - 3.0f, y), "left of the track is no level");
    expect(!chooser.iconSizeAt(track.centreX(), chooser.panel.y - 4.0f), "above the panel is none");
}

void libraryHoldsEveryRowAndHomeOnlyItsOwn() {
    ModeChooser chooser;
    chooser.open(LibraryMode::Standard, ChooserValues{}, true);
    expect(chooser.rows() == libraryRows(), "Library holds the layout rows then the view rows");
    chooser.open(LibraryMode::Standard, ChooserValues{}, false);
    expect(chooser.rows() == std::vector<ChooserRow>{ChooserRow::Sort, ChooserRow::Source,
                                                     ChooserRow::Installed, ChooserRow::Hidden,
                                                     ChooserRow::Search, ChooserRow::Settings},
           "Home holds the sort, the filters, the search and the settings");
    expect(chooser.row() == ChooserRow::Sort, "and opens on the first of them");
    expect(!chooser.focusRow(ChooserRow::Pin), "a row Home lacks cannot be focused");
}

void rowFocusMoves() {
    ModeChooser chooser;
    chooser.open(LibraryMode::Xmb, ChooserValues{}, true);
    expect(chooser.row() == ChooserRow::Cards && chooser.focused() == LibraryMode::Xmb,
           "it opens on the cards with the mode in use");
    expect(chooser.moveRow(1) && chooser.row() == ChooserRow::IconSize, "down to the slider");
    expect(chooser.moveRow(1) && chooser.row() == ChooserRow::Pin, "then the pin row");
    expect(chooser.moveRow(-1) && chooser.row() == ChooserRow::IconSize, "and back");
    for (int i = 0; i < 20; ++i) {
        chooser.moveRow(1);
    }
    expect(chooser.row() == ChooserRow::Settings && !chooser.moveRow(1),
           "it stops at the last row");
    expect(chooser.focusRow(ChooserRow::Hidden) && !chooser.focusRow(ChooserRow::Hidden),
           "the pointer focuses a row once");
    chooser.open(LibraryMode::Xmb, ChooserValues{}, true);
    expect(chooser.row() == ChooserRow::Cards, "a reopened panel starts on the cards");
}

void cardFocusMoves() {
    ModeChooser chooser;
    expect(!chooser.isOpen(), "closed at first");
    chooser.open(LibraryMode::Xmb, ChooserValues{}, true);
    expect(chooser.isOpen(), "open");
    expect(chooser.move(1) && chooser.focused() == LibraryMode::Carousel, "right");
    expect(!chooser.move(1) && chooser.focused() == LibraryMode::Carousel, "it stops at the end");
    expect(chooser.move(-1) && chooser.move(-1) && chooser.focused() == LibraryMode::Standard,
           "left");
    expect(!chooser.move(-1), "it stops at the start");
    expect(chooser.move(5) && chooser.focused() == LibraryMode::Carousel, "a long move clamps");
    expect(chooser.moveRow(1) && chooser.focus(LibraryMode::Carousel) &&
               chooser.row() == ChooserRow::Cards,
           "a card taken by the pointer pulls focus back to the cards");
    expect(!chooser.focus(LibraryMode::Carousel), "the focused card is not a move");
    chooser.close();
    expect(!chooser.isOpen(), "closed again");
}

} // namespace

int main() {
    cardsKeepIisusMeasures();
    cardsUnderThePointer();
    panelStaysInTheFrame();
    focusedRowScrollsIntoView();
    rowsUnderThePointer();
    sliderMapsPositionsToLevels();
    libraryHoldsEveryRowAndHomeOnlyItsOwn();
    rowFocusMoves();
    cardFocusMoves();
    std::printf("mode_chooser: all checks passed\n");
    return 0;
}

// The Library layout picker's focus.
#include "mode_chooser.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::library::LibraryMode;
using opensu::test::expect;
using opensu::test::near;
using opensu::ui::ModeChooser;

void layout() {
    // navigation.md §5.2 on 1920 x 1080 at 2.25 px per dp: cards 352 px square, 24 px apart.
    const opensu::ui::ChooserLayout chooser =
        opensu::ui::layoutChooser(opensu::ui::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, 2.25f);
    near(chooser.panel.width, 1178.1, "the panel is 523.6 dp wide", 0.1);
    near(chooser.panel.centreX(), 960.0, "centred", 0.01);
    near(chooser.cards[0].width, 352.3, "cards are 352 px", 0.5);
    near(chooser.cards[0].height, chooser.cards[0].width, "square", 0.01);
    near(chooser.cards[1].x - chooser.cards[0].right(), 24.1, "24 px apart", 0.1);
    near(chooser.cards[0].x - chooser.panel.x, 36.0, "36 px in from the side", 0.01);
    near(chooser.cards[0].y - chooser.panel.y, 47.25, "47 px down from the top", 0.01);
    near(chooser.panel.right() - chooser.cards[2].right(), 36.0, "and 36 px from the other side",
         0.01);
    near(chooser.panel.centreY(), 540.0, "centred vertically", 0.01);
}

void cardsUnderThePointer() {
    const opensu::ui::ChooserLayout chooser =
        opensu::ui::layoutChooser(opensu::ui::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, 2.25f);
    for (std::size_t i = 0; i < chooser.cards.size(); ++i) {
        const auto hit = chooser.cardAt(chooser.cards[i].centreX(), chooser.cards[i].centreY());
        expect(hit && *hit == opensu::library::allLibraryModes[i], "a card's centre hits it");
    }
    const opensu::ui::Rect& left = chooser.cards[0];
    const opensu::ui::Rect& middle = chooser.cards[1];
    expect(!chooser.cardAt((left.right() + middle.x) * 0.5f, left.centreY()),
           "the gap between cards hits none");
    expect(!chooser.cardAt(chooser.panel.x + 1.0f, chooser.panel.y + 1.0f),
           "the panel's padding hits none");
}

void focusByPointer() {
    ModeChooser chooser;
    chooser.open(LibraryMode::Standard);
    expect(chooser.focus(LibraryMode::Carousel) && chooser.focused() == LibraryMode::Carousel,
           "a card takes focus");
    expect(!chooser.focus(LibraryMode::Carousel), "the focused card is not a move");
}

void focusMoves() {
    ModeChooser chooser;
    expect(!chooser.isOpen(), "closed at first");
    chooser.open(LibraryMode::Xmb);
    expect(chooser.isOpen() && chooser.focused() == LibraryMode::Xmb,
           "it opens on the mode in use");
    expect(chooser.move(1) && chooser.focused() == LibraryMode::Carousel, "right");
    expect(!chooser.move(1) && chooser.focused() == LibraryMode::Carousel, "it stops at the end");
    expect(chooser.move(-1) && chooser.move(-1) && chooser.focused() == LibraryMode::Standard,
           "left");
    expect(!chooser.move(-1) && chooser.focused() == LibraryMode::Standard,
           "it stops at the start");
    expect(chooser.move(5) && chooser.focused() == LibraryMode::Carousel, "a long move clamps");
    chooser.close();
    expect(!chooser.isOpen(), "closed again");
    chooser.open(LibraryMode::Standard);
    expect(chooser.focused() == LibraryMode::Standard, "it reopens on the current mode");
}

} // namespace

int main() {
    layout();
    cardsUnderThePointer();
    focusByPointer();
    focusMoves();
    std::printf("mode_chooser: all checks passed\n");
    return 0;
}

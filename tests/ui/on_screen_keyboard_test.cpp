// The on-screen keyboard: what its keys type, the regular grid's D-pad walk and capitals, the daisy
// wheel's petals and sets, and the docked layouts the pointer hit-tests.
#include "on_screen_keyboard.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::input::KeyboardStyle;
using opensu::test::expect;

std::size_t indexOf(KeyKind kind, char character = '\0') {
    const auto keys = gridKeys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].kind == kind &&
            (kind != KeyKind::Character || keys[i].character == character)) {
            return i;
        }
    }
    expect(false, "key exists");
    return 0;
}

KeyPress pressKey(OnScreenKeyboard& keyboard, KeyKind kind, char character = '\0') {
    keyboard.focusKey(indexOf(kind, character));
    return keyboard.press();
}

OnScreenKeyboard opened(KeyboardStyle style) {
    OnScreenKeyboard keyboard;
    keyboard.setStyle(style);
    keyboard.open();
    return keyboard;
}

void gridKeysType() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular);
    expect(keyboard.isOpen() && !keyboard.shifted(), "opens without capitals");
    expect(pressKey(keyboard, KeyKind::Character, 'h').typed == "h", "a letter key types it");
    expect(pressKey(keyboard, KeyKind::Character, '/').typed == "/", "the slash types, for paths");
    expect(pressKey(keyboard, KeyKind::Space).typed == " ", "space types a space");
    const KeyPress erase = pressKey(keyboard, KeyKind::Backspace);
    expect(erase.erase && erase.typed.empty() && !erase.done, "Delete erases");
    expect(pressKey(keyboard, KeyKind::Done).done, "Done is done");
    keyboard.toggleShift();
    expect(pressKey(keyboard, KeyKind::Character, 'b').typed == "B" &&
               pressKey(keyboard, KeyKind::Character, '1').typed == "1",
           "capitals apply to letters only");
    keyboard.close();
    keyboard.open();
    expect(!keyboard.shifted(), "opening again starts without capitals");
}

void dpadWalksTheGrid() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular);
    expect(keyboard.keyFocus() == 0, "starts on the first key");
    expect(!keyboard.move(Direction::Left) && !keyboard.move(Direction::Up), "edges stop the walk");
    expect(keyboard.move(Direction::Right) && keyboard.keyFocus() == 1, "right steps");
    expect(keyboard.move(Direction::Down) && gridKeys()[keyboard.keyFocus()].row == 1,
           "down a row");
    for (int i = 0; i < 3; ++i) {
        keyboard.move(Direction::Down);
    }
    expect(gridKeys()[keyboard.keyFocus()].row == 4, "the bottom row");
    expect(!keyboard.move(Direction::Down), "no row below the last");
    const std::size_t space = indexOf(KeyKind::Space);
    keyboard.focusKey(space);
    keyboard.move(Direction::Up);
    const std::size_t above = keyboard.keyFocus();
    keyboard.move(Direction::Down);
    expect(keyboard.keyFocus() == space || gridKeys()[keyboard.keyFocus()].row == 4,
           "down returns to the bottom row");
    expect(above < space, "up goes to an earlier key");
}

void theStickPicksAPetal() {
    expect(!petalPointedAt(false, false, false, false), "a centred stick picks nothing");
    expect(petalPointedAt(true, false, false, false) == std::optional<std::size_t>{0},
           "up is the first petal");
    expect(petalPointedAt(true, false, false, true) == std::optional<std::size_t>{1},
           "up and right is the next, clockwise");
    expect(petalPointedAt(false, false, true, false) == std::optional<std::size_t>{6}, "left");
    expect(petalPointedAt(true, false, true, false) == std::optional<std::size_t>{7}, "up-left");

    OnScreenKeyboard keyboard = opened(KeyboardStyle::DaisyWheel);
    expect(!keyboard.petal() && keyboard.typeFace(DaisyFace::A).empty(), "no petal types nothing");
    expect(keyboard.stick(Direction::Up, true) && keyboard.petal() == std::optional<std::size_t>{0},
           "pushing up picks the top petal");
    expect(keyboard.typeFace(DaisyFace::X) == "a" && keyboard.typeFace(DaisyFace::A) == "d",
           "the face buttons type the petal's characters");
    keyboard.stick(Direction::Right, true);
    expect(keyboard.petal() == std::optional<std::size_t>{1}, "a diagonal picks between");
    expect(keyboard.typeFace(DaisyFace::Y) == "f", "its characters");
    keyboard.stick(Direction::Up, false);
    keyboard.stick(Direction::Right, false);
    expect(!keyboard.petal(), "letting go picks nothing");
}

void theSetsCycle() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::DaisyWheel);
    expect(keyboard.set() == DaisySet::Letters, "letters first");
    keyboard.cycleSet(1);
    expect(keyboard.set() == DaisySet::Capitals, "RT steps on");
    keyboard.stick(Direction::Up, true);
    const std::string capital = keyboard.typeFace(DaisyFace::X);
    keyboard.cycleSet(1);
    expect(capital == "A" && keyboard.typeFace(DaisyFace::X) == "1",
           "each set types its own characters");
    keyboard.cycleSet(-1);
    keyboard.cycleSet(-1);
    keyboard.cycleSet(-1);
    expect(keyboard.set() == DaisySet::Symbols, "LT wraps back");
    for (std::size_t set = 0; set < daisySets; ++set) {
        expect(!daisySetLabel(static_cast<DaisySet>(set)).empty(), "every set has a label");
    }
    keyboard.close();
    keyboard.open();
    expect(keyboard.set() == DaisySet::Letters && !keyboard.petal(),
           "opening again starts on letters with no petal");
}

void thePointerTypesAWheelCharacter() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::DaisyWheel);
    const std::size_t yFace = daisyPetals + static_cast<std::size_t>(DaisyFace::Y);
    expect(keyboard.focusKey(1) && keyboard.pointed(), "the pointer rests on a petal");
    expect(keyboard.petal() == std::optional<std::size_t>{1}, "which picks it");
    expect(keyboard.press().typed.empty(), "a click on a petal types nothing");
    keyboard.focusKey(yFace);
    expect(keyboard.petal() == std::optional<std::size_t>{1},
           "the petal stays picked while the pointer moves to its characters");
    expect(keyboard.press().typed == "f", "a click types that character");
    expect(!keyboard.pointed() && keyboard.press().typed.empty(), "once");
    keyboard.focusKey(yFace);
    expect(keyboard.press().typed == "f", "and again on the next click");
    keyboard.stick(Direction::Down, true);
    expect(!keyboard.pointed() && keyboard.petal() == std::optional<std::size_t>{4},
           "the stick takes the petal from the pointer");
    keyboard.stick(Direction::Down, false);
    expect(!keyboard.petal(), "and lets go of it");
    expect(!keyboard.focusKey(daisyKeys), "no key past the wheel");
}

void styleTakesEffectOnOpen() {
    OnScreenKeyboard keyboard;
    expect(keyboard.style() == KeyboardStyle::DaisyWheel, "the daisy wheel by default");
    keyboard.open();
    keyboard.setStyle(KeyboardStyle::Regular);
    expect(keyboard.style() == KeyboardStyle::DaisyWheel, "an open keyboard keeps its style");
    keyboard.close();
    keyboard.open();
    expect(keyboard.style() == KeyboardStyle::Regular, "the next open takes the new one");
}

void layoutsHitTest() {
    for (const Rect frame : {Rect{0.0f, 0.0f, 1280.0f, 800.0f}, Rect{0.0f, 0.0f, 1920.0f, 1080.0f},
                             Rect{0.0f, 0.0f, 400.0f, 300.0f}}) {
        const float dp = std::min(frame.width / 853.0f, frame.height / 480.0f);
        for (const KeyboardStyle style : opensu::input::allKeyboardStyles) {
            const KeyboardLayout layout = layoutKeyboard(frame, dp, style);
            const std::size_t keys =
                style == KeyboardStyle::Regular ? gridKeys().size() : daisyKeys;
            expect(layout.keys.size() == keys, "a rectangle per key");
            expect(layout.panel.x >= 0.0f && layout.panel.right() <= frame.width &&
                       layout.panel.bottom() <= frame.height,
                   "the dock fits the frame");
            expect(frame.height - layout.panel.y <= frame.height / 5.0f + 0.01f,
                   "and covers at most the bottom fifth");
            expect(std::abs(layout.panel.centreX() - frame.centreX()) < 0.01f, "centred");
            expect(layout.strip.bottom() <= layout.hints.y, "the strip stands over the hints");
            for (std::size_t i = 0; i < layout.keys.size(); ++i) {
                expect(layout.keys[i].y >= layout.panel.y &&
                           layout.keys[i].bottom() <= layout.panel.bottom() &&
                           (layout.keys[i].right() <= layout.strip.x ||
                            layout.keys[i].x >= layout.strip.right()),
                       "keys sit in the dock beside the strip and hints");
            }
        }
    }
    const KeyboardLayout wheel =
        layoutKeyboard(Rect{0.0f, 0.0f, 1280.0f, 800.0f}, 1.5f, KeyboardStyle::DaisyWheel);
    expect(wheel.petalBounds(0).centreY() < wheel.ring.centreY() &&
               wheel.petalBounds(2).centreX() > wheel.ring.centreX(),
           "the first petal is on top and the third on the right");
    expect(wheel.faceBounds(DaisyFace::Y).centreY() < wheel.diamond.centreY() &&
               wheel.faceBounds(DaisyFace::X).centreX() < wheel.diamond.centreX() &&
               wheel.ring.right() <= wheel.strip.x && wheel.strip.right() <= wheel.diamond.x,
           "the ring on the left, the diamond on the right with Y on top and X on the left");
}

} // namespace

int main() {
    gridKeysType();
    dpadWalksTheGrid();
    theStickPicksAPetal();
    theSetsCycle();
    thePointerTypesAWheelCharacter();
    styleTakesEffectOnOpen();
    layoutsHitTest();
    std::printf("on_screen_keyboard: all checks passed\n");
    return 0;
}

// The on-screen keyboard: the line and its cursor, the regular grid's D-pad walk, the daisy
// wheel's petals and sets, a secret line, and the docked layouts the pointer hit-tests.
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

OnScreenKeyboard opened(KeyboardStyle style, EntryKind kind = EntryKind::Search,
                        std::string text = {}) {
    OnScreenKeyboard keyboard;
    keyboard.setStyle(style);
    keyboard.configure("Type", kind);
    keyboard.open(std::move(text));
    return keyboard;
}

void gridKeysType() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular);
    expect(keyboard.isOpen() && keyboard.text().empty(), "opens empty");
    expect(pressKey(keyboard, KeyKind::Character, 'h').edited && keyboard.text() == "h",
           "a letter types");
    pressKey(keyboard, KeyKind::Character, 'i');
    expect(pressKey(keyboard, KeyKind::Space).edited && keyboard.text() == "hi ", "space types");
    expect(!pressKey(keyboard, KeyKind::Space).edited, "a search never doubles a space");
    pressKey(keyboard, KeyKind::Character, 'x');
    expect(keyboard.text() == "hi x", "text accumulates");
    expect(pressKey(keyboard, KeyKind::Backspace).edited && keyboard.text() == "hi ",
           "delete removes one");
    expect(pressKey(keyboard, KeyKind::Clear).edited && keyboard.text().empty(), "clear empties");
    expect(!pressKey(keyboard, KeyKind::Clear).edited, "clearing nothing is no edit");
    expect(pressKey(keyboard, KeyKind::Done).done, "done is done");
}

void theLineAndItsLimits() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular, EntryKind::Search, "ab");
    expect(keyboard.cursor() == 2, "the cursor starts after the text");
    expect(keyboard.type("c\xC3\xA9") && keyboard.text() == "abc\xC3\xA9", "typed text appends");
    expect(keyboard.backspace() && keyboard.text() == "abc", "backspace removes a whole character");
    expect(!keyboard.type(""), "empty text is no edit");
    expect(keyboard.moveCursor(-1) && keyboard.moveCursor(-1) && keyboard.cursor() == 1,
           "the cursor moves left a character at a time");
    expect(keyboard.type("X") && keyboard.text() == "aXbc", "typing inserts at the cursor");
    expect(keyboard.backspace() && keyboard.text() == "abc" && keyboard.cursor() == 1,
           "backspace deletes before the cursor");
    keyboard.moveCursor(-1);
    expect(!keyboard.moveCursor(-1) && !keyboard.backspace(), "nothing before the start");
    expect(!keyboard.typeSpace(), "a search never starts with a space");
    keyboard.clear();
    keyboard.type(std::string(searchMostLength, 'a'));
    expect(!keyboard.type("b") && keyboard.text().size() == searchMostLength,
           "the field is bounded");
    OnScreenKeyboard path = opened(KeyboardStyle::Regular, EntryKind::Path);
    expect(path.type(std::string(searchMostLength + 1, 'a')),
           "a typed path may be longer than a search");
    path.clear();
    expect(!path.type(std::string(pathMostLength + 1, 'a')), "but not longer than a path can be");
    expect(path.typeSpace() && path.typeSpace(), "a path may hold any spaces");
}

void dpadWalksTheGrid() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular);
    expect(keyboard.keyFocus() == 0 && keyboard.onTopRow(), "starts on the first key");
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
    expect(!keyboard.petal() && !keyboard.typeFace(DaisyFace::A), "no petal types nothing");
    expect(keyboard.stick(Direction::Up, true) && keyboard.petal() == std::optional<std::size_t>{0},
           "pushing up picks the top petal");
    expect(keyboard.typeFace(DaisyFace::X) && keyboard.typeFace(DaisyFace::A) &&
               keyboard.text() == "ad",
           "the face buttons type the petal's characters");
    keyboard.stick(Direction::Right, true);
    expect(keyboard.petal() == std::optional<std::size_t>{1}, "a diagonal picks between");
    keyboard.typeFace(DaisyFace::Y);
    expect(keyboard.text() == "adf", "its characters");
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
    keyboard.typeFace(DaisyFace::X);
    keyboard.cycleSet(1);
    keyboard.typeFace(DaisyFace::X);
    expect(keyboard.text() == "A1", "each set types its own characters");
    keyboard.cycleSet(-1);
    keyboard.cycleSet(-1);
    keyboard.cycleSet(-1);
    expect(keyboard.set() == DaisySet::Symbols, "LT wraps back");
    for (std::size_t set = 0; set < daisySets; ++set) {
        expect(!daisySetLabel(static_cast<DaisySet>(set)).empty(), "every set has a label");
    }
    keyboard.close();
    keyboard.open({});
    expect(keyboard.set() == DaisySet::Letters && !keyboard.petal(),
           "opening again starts on letters with no petal");
}

void thePointerTypesAWheelCharacter() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::DaisyWheel);
    const std::size_t yFace = daisyPetals + static_cast<std::size_t>(DaisyFace::Y);
    expect(keyboard.focusKey(1) && keyboard.pointed(), "the pointer rests on a petal");
    expect(keyboard.petal() == std::optional<std::size_t>{1}, "which picks it");
    expect(!keyboard.press().edited && keyboard.text().empty(), "a click on a petal types nothing");
    keyboard.focusKey(yFace);
    expect(keyboard.petal() == std::optional<std::size_t>{1},
           "the petal stays picked while the pointer moves to its characters");
    expect(keyboard.press().edited && keyboard.text() == "f", "a click types that character");
    expect(!keyboard.pointed(), "once");
    keyboard.focusKey(yFace);
    expect(keyboard.press().edited && keyboard.text() == "ff", "and again on the next click");
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
    keyboard.configure("Type", EntryKind::Search);
    keyboard.open({});
    keyboard.setStyle(KeyboardStyle::Regular);
    expect(keyboard.style() == KeyboardStyle::DaisyWheel, "an open keyboard keeps its style");
    keyboard.close();
    keyboard.open({});
    expect(keyboard.style() == KeyboardStyle::Regular, "the next open takes the new one");
}

void aSecretLineWipesAndCapitalises() {
    OnScreenKeyboard keyboard = opened(KeyboardStyle::Regular, EntryKind::Password);
    expect(keyboard.secret() && !keyboard.shifted(), "a secret entry starts without capitals");
    pressKey(keyboard, KeyKind::Character, 'a');
    keyboard.toggleShift();
    pressKey(keyboard, KeyKind::Character, 'b');
    pressKey(keyboard, KeyKind::Character, '1');
    expect(keyboard.text() == "aB1", "capitals apply to letters only");
    expect(keyboard.type("x") && keyboard.text() == "aB1x", "a physical keyboard types too");
    keyboard.moveCursor(-1);
    keyboard.backspace();
    keyboard.close();
    expect(keyboard.text().empty() && !keyboard.isOpen() && keyboard.cursor() == 0,
           "closing empties the line");
    const char* buffer = keyboard.text().data();
    expect(buffer[0] == '\0' && buffer[1] == '\0' && buffer[2] == '\0' && buffer[3] == '\0',
           "and overwrites everything typed, a deleted character included");
    keyboard.configure("Administrator password", EntryKind::Password);
    keyboard.open({});
    expect(!keyboard.shifted(), "configuring starts without capitals again");
    pressKey(keyboard, KeyKind::Character, 'q');
    const std::string taken = keyboard.takeText();
    expect(taken == "q" && keyboard.text().empty() && keyboard.cursor() == 0,
           "the text can be taken out");
    OnScreenKeyboard plain = opened(KeyboardStyle::Regular, EntryKind::Path);
    expect(!plain.secret(), "an ordinary entry is not secret");
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
            expect(layout.preview.height > 0.0f && layout.preview.bottom() <= layout.strip.y &&
                       layout.strip.bottom() <= layout.hints.y,
                   "the preview stands over the strip and the hints");
            for (std::size_t i = 0; i < layout.keys.size(); ++i) {
                const auto hit = layout.keyAt(layout.keys[i].centreX(), layout.keys[i].centreY());
                expect(hit && *hit == i, "a key's centre hits that key");
                expect(layout.keys[i].y >= layout.panel.y &&
                           layout.keys[i].bottom() <= layout.panel.bottom() &&
                           (layout.keys[i].right() <= layout.strip.x ||
                            layout.keys[i].x >= layout.strip.right()),
                       "keys sit in the dock beside the strip and hints");
            }
            expect(!layout.keyAt(1.0f, 1.0f), "outside hits nothing");
            expect(layoutKeyboard(frame, dp, style, false).preview.height == 0.0f,
                   "a caller with its own field takes no preview");
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
    theLineAndItsLimits();
    dpadWalksTheGrid();
    theStickPicksAPetal();
    theSetsCycle();
    thePointerTypesAWheelCharacter();
    styleTakesEffectOnOpen();
    aSecretLineWipesAndCapitalises();
    layoutsHitTest();
    std::printf("on_screen_keyboard: all checks passed\n");
    return 0;
}

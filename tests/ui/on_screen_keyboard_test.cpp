// The on-screen keyboard: the line and its cursor, the regular grid's D-pad walk, the daisy
// wheel's petals and sets, a secret line, and the layouts the pointer hit-tests.
#include "on_screen_keyboard.hpp"

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
    const std::size_t yOnSecond = 1 * daisyFaces + static_cast<std::size_t>(DaisyFace::Y);
    expect(keyboard.focusKey(yOnSecond) && keyboard.pointed(), "the pointer rests on a character");
    expect(keyboard.petal() == std::optional<std::size_t>{1}, "which picks its petal");
    expect(keyboard.press().edited && keyboard.text() == "f", "a click types that character");
    expect(!keyboard.pointed() && !keyboard.petal(), "once");
    keyboard.focusKey(yOnSecond);
    keyboard.stick(Direction::Down, true);
    expect(!keyboard.pointed() && keyboard.petal() == std::optional<std::size_t>{4},
           "the stick takes the petal from the pointer");
    expect(!keyboard.focusKey(daisyPetals * daisyFaces), "no character past the wheel");
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
    const Rect frame{0.0f, 0.0f, 1280.0f, 720.0f};
    for (const KeyboardStyle style : opensu::input::allKeyboardStyles) {
        const KeyboardLayout layout = layoutKeyboard(frame, 1.0f, style, 84.0f);
        const std::size_t keys =
            style == KeyboardStyle::Regular ? gridKeys().size() : daisyPetals * daisyFaces;
        expect(layout.keys.size() == keys, "a rectangle per key");
        expect(layout.panel.x >= 0.0f && layout.panel.right() <= frame.width &&
                   layout.panel.y >= 0.0f && layout.panel.bottom() <= frame.height,
               "the panel fits the frame");
        expect(layout.reserved.height == 84.0f && layout.reserved.y >= layout.field.bottom(),
               "the reserved room sits under the field");
        for (std::size_t i = 0; i < layout.keys.size(); ++i) {
            const auto hit = layout.keyAt(layout.keys[i].centreX(), layout.keys[i].centreY());
            expect(hit && *hit == i, "a key's centre hits that key");
            expect(layout.keys[i].y >= layout.reserved.bottom() &&
                       layout.keys[i].bottom() <= layout.hints.y,
                   "keys sit between the reserved room and the hints");
        }
        expect(!layout.keyAt(1.0f, 1.0f), "outside hits nothing");
    }
    const KeyboardLayout wheel = layoutKeyboard(frame, 1.0f, KeyboardStyle::DaisyWheel);
    expect(wheel.panel.height == layoutKeyboard(frame, 1.0f, KeyboardStyle::Regular).panel.height,
           "both styles stand as tall, so neither covers more of the screen");
    expect(wheel.petalBounds(0).centreY() < wheel.wheel.centreY() &&
               wheel.petalBounds(2).centreX() > wheel.wheel.centreX(),
           "the first petal is on top and the third on the right");
    const KeyboardLayout narrow =
        layoutKeyboard(Rect{0.0f, 0.0f, 400.0f, 300.0f}, 1.0f, KeyboardStyle::Regular);
    expect(narrow.panel.width <= 400.0f, "the panel never exceeds a narrow frame");
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

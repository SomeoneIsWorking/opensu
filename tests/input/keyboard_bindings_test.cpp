// The keys the shell binds, and the cap each prompt shows for them.
#include "input/keyboard_bindings.hpp"

#include <cstdio>
#include <string>

#include "check.hpp"
#include "raylib.h"

namespace {

using opensu::gamepad::Button;
using opensu::input::buttonOfGlyph;
using opensu::input::keyBindings;
using opensu::input::keyLabelFor;
using opensu::input::keyLabelForGlyph;
using opensu::test::expect;

void promptsNameTheBoundKeys() {
    expect(keyLabelForGlyph("A") == "Enter", "A confirms with Enter");
    expect(keyLabelForGlyph("B") == "Esc", "B backs out with Escape");
    expect(keyLabelForGlyph("X") == "F", "X refreshes with F");
    expect(keyLabelForGlyph("Y") == "Y", "Y shows details with Y");
    expect(keyLabelForGlyph("-") == "Tab", "the details prompt, Select, is Tab");
    expect(keyLabelForGlyph("+") == "E", "the menu prompt, Start, is E");
    expect(keyLabelForGlyph("LB") == "[" && keyLabelForGlyph("RB") == "]",
           "the shoulders are the brackets");
    expect(keyLabelFor(Button::Up) == "Up" && keyLabelFor(Button::Left) == "Left",
           "a direction shows its arrow before its letter");
}

void unboundGlyphsHaveNoCap() {
    expect(!keyLabelForGlyph("LT") && !keyLabelForGlyph(""), "an unknown glyph has no key");
    expect(!keyLabelFor(Button::Guide), "a button no key reaches has no cap");
    expect(buttonOfGlyph("RB") == Button::R1 && buttonOfGlyph("Q") == Button::None,
           "glyph keys map to buttons");
}

void everyLabelMatchesItsTable() {
    for (const auto& binding : keyBindings()) {
        const auto label = keyLabelFor(binding.button);
        expect(label.has_value() && !label->empty(), "every bound button has a label");
    }
    // The first binding of a button is the one its prompt shows.
    for (const auto& binding : keyBindings()) {
        if (binding.button == Button::R1) {
            expect(binding.key == KEY_RIGHT_BRACKET, "RB's first key is the shown one");
            break;
        }
    }
}

void searchHasSlashAndControlF() {
    bool slash = false;
    bool controlF = false;
    for (const auto& binding : keyBindings()) {
        if (binding.button != Button::Search) {
            continue;
        }
        slash = slash || (binding.key == KEY_SLASH && !binding.ctrl);
        controlF = controlF || (binding.key == KEY_F && binding.ctrl);
    }
    expect(slash && controlF, "/ and Ctrl+F open the search");
    expect(keyLabelFor(Button::Search) == "/", "the search's cap is the slash");
    for (const auto& binding : keyBindings()) {
        if (binding.key == KEY_F && !binding.ctrl) {
            expect(binding.button == Button::X, "a bare F is still X");
        }
    }
}

} // namespace

int main() {
    promptsNameTheBoundKeys();
    unboundGlyphsHaveNoCap();
    everyLabelMatchesItsTable();
    searchHasSlashAndControlF();
    std::puts("keyboard_bindings: ok");
    return 0;
}

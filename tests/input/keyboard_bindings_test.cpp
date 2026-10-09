// The keys the shell binds, and the cap each prompt shows for them.
#include "input/keyboard_bindings.hpp"

#include <cstdio>
#include <string>

#include "check.hpp"
#include "raylib.h"

namespace {

using iideck::gamepad::Button;
using iideck::input::buttonOfGlyph;
using iideck::input::keyBindings;
using iideck::input::keyLabelFor;
using iideck::input::keyLabelForGlyph;
using iideck::test::expect;

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

} // namespace

int main() {
    promptsNameTheBoundKeys();
    unboundGlyphsHaveNoCap();
    everyLabelMatchesItsTable();
    std::puts("keyboard_bindings: ok");
    return 0;
}

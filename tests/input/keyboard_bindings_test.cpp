// The keys the shell binds, and the cap each prompt shows for them.
#include "input/keyboard_bindings.hpp"

#include <cstdio>
#include <set>
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

/// A keyboard that holds the keys it is given: `held` are down, `fresh` went down this frame.
class FakeKeys final : public opensu::input::KeySource {
  public:
    FakeKeys(std::set<int> held, std::set<int> fresh)
        : held_{std::move(held)}, fresh_{std::move(fresh)} {
    }
    bool down(int key) const override {
        return held_.contains(key);
    }
    bool pressed(int key) const override {
        return fresh_.contains(key);
    }
    bool released(int) const override {
        return false;
    }

  private:
    std::set<int> held_;
    std::set<int> fresh_;
};

bool presses(const FakeKeys& keys, Button button) {
    for (const auto& event : opensu::input::keyEvents(keys)) {
        if (event.button == button && event.pressed) {
            return true;
        }
    }
    return false;
}

void contextMenuKeys() {
    expect(presses(FakeKeys{{KEY_TAB}, {KEY_TAB}}, Button::Select),
           "Tab asks for the context menu");
    expect(presses(FakeKeys{{KEY_KB_MENU}, {KEY_KB_MENU}}, Button::Select),
           "the Menu key asks for the context menu");
    expect(presses(FakeKeys{{KEY_LEFT_SHIFT, KEY_F10}, {KEY_F10}}, Button::Select),
           "Shift+F10 asks for the context menu");
    expect(!presses(FakeKeys{{KEY_F10}, {KEY_F10}}, Button::Select), "a bare F10 does not");
    expect(!presses(FakeKeys{{KEY_LEFT_SHIFT, KEY_TAB}, {KEY_TAB}}, Button::Select),
           "Shift+Tab is Guide's chord and does not open the menu");
    expect(presses(FakeKeys{{KEY_ENTER}, {KEY_ENTER}}, Button::A), "Enter is A");
    expect(presses(FakeKeys{{KEY_LEFT_CONTROL, KEY_F}, {KEY_F}}, Button::Search) &&
               !presses(FakeKeys{{KEY_LEFT_CONTROL, KEY_F}, {KEY_F}}, Button::X),
           "Ctrl+F is the search, not X");
    expect(keyLabelFor(Button::Select) == "Tab", "the context menu's prompt names Tab");
}

} // namespace

int main() {
    contextMenuKeys();
    promptsNameTheBoundKeys();
    unboundGlyphsHaveNoCap();
    everyLabelMatchesItsTable();
    searchHasSlashAndControlF();
    std::puts("keyboard_bindings: ok");
    return 0;
}

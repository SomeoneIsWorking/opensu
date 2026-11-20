// Shift+Tab as Guide: the chord a game's key edges complete.
#include "input/shortcut_chord.hpp"

#include <cstdio>
#include <cstdlib>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::input::Combo;
using opensu::input::ShortcutChord;

int code(const char* name) {
    return opensu::test::need(opensu::input::comboNamed(name), "a key name").key;
}

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void chordEdges() {
    const int leftShift = opensu::input::modifierKeys()[0];
    const int rightShift = opensu::input::modifierKeys()[1];
    const int leftControl = opensu::input::modifierKeys()[2];
    ShortcutChord chord;
    const Combo shiftTab{code("tab"), false, true};
    expect(chord.key(code("tab"), true) == Combo{code("tab")}, "Tab alone is a bare Tab");
    expect(!chord.key(code("tab"), false), "Tab release fires nothing");
    expect(!chord.key(leftShift, true), "Shift alone is no combination");
    expect(chord.key(code("tab"), true) == shiftTab, "Shift+Tab");
    expect(!chord.key(code("tab"), true), "a held Tab's repeat fires nothing");
    expect(!chord.key(code("tab"), false), "releasing Tab fires nothing");
    expect(chord.key(code("tab"), true) == shiftTab, "a second tap fires again");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(leftShift, false), "Shift up");
    expect(chord.key(code("tab"), true) == Combo{code("tab")},
           "Tab after Shift is released is bare");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(rightShift, true), "right Shift down");
    expect(chord.key(code("tab"), true) == shiftTab, "right Shift+Tab");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(rightShift, false), "Shift up");
    expect(!chord.key(leftControl, true), "Ctrl down");
    expect(chord.key(code("up"), true) == Combo{code("up"), true, false}, "Ctrl+Up");
}

} // namespace

int main() try {
    chordEdges();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

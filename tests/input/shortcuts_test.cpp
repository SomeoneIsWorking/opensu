// The shortcut table: the shipped defaults (the keys the shell has always bound), the caps the
// prompts show, remapping with its conflict checks, and pad chords told from plain presses.
#include "input/shortcuts.hpp"

#include <cstdio>
#include <set>
#include <string>

#include "check.hpp"
#include "input/keyboard_bindings.hpp"
#include "input/pad_chords.hpp"
#include "raylib.h"

namespace {

using namespace opensu::input;
using opensu::gamepad::Button;
using opensu::gamepad::Event;
using opensu::test::expect;
using opensu::test::need;

void promptsNameTheBoundKeys() {
    const Shortcuts shortcuts;
    expect(shortcuts.keyLabelForGlyph("A") == "Enter", "A confirms with Enter");
    expect(shortcuts.keyLabelForGlyph("B") == "Esc", "B backs out with Escape");
    expect(shortcuts.keyLabelForGlyph("X") == "F", "X refreshes with F");
    expect(shortcuts.keyLabelForGlyph("Y") == "Y", "Y shows details with Y");
    expect(shortcuts.keyLabelForGlyph("-") == "Tab", "the details prompt, Select, is Tab");
    expect(shortcuts.keyLabelForGlyph("+") == "E", "the menu prompt, Start, is E");
    expect(shortcuts.keyLabelForGlyph("LB") == "[" && shortcuts.keyLabelForGlyph("RB") == "]",
           "the shoulders are the brackets");
    expect(shortcuts.keyLabelFor(Button::Up) == "Up" &&
               shortcuts.keyLabelFor(Button::Left) == "Left",
           "a direction shows its arrow before its letter");
    expect(shortcuts.keyLabelFor(Button::Search) == "/", "the search's cap is the slash");
    expect(shortcuts.keyLabelFor(Button::Guide) == "Shift+Tab", "Guide's cap is its chord");
    expect(!shortcuts.keyLabelForGlyph("LT") && !shortcuts.keyLabelForGlyph(""),
           "an unknown glyph has no key");
    expect(buttonOfGlyph("RB") == Button::R1 && buttonOfGlyph("Q") == Button::None,
           "glyph keys map to buttons");
}

/// A keyboard that holds the keys it is given: `held` are down, `fresh` went down this frame.
class FakeKeys final : public KeySource {
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

bool presses(const Shortcuts& shortcuts, const FakeKeys& keys, Action action) {
    for (const ActionEdge& edge : shortcuts.keyEdges(keys)) {
        if (edge.action == action && edge.pressed) {
            return true;
        }
    }
    return false;
}

void shippedKeysAreUnchanged() {
    const Shortcuts s;
    expect(presses(s, FakeKeys{{KEY_TAB}, {KEY_TAB}}, Action::Select), "Tab asks for the menu");
    expect(presses(s, FakeKeys{{KEY_KB_MENU}, {KEY_KB_MENU}}, Action::Select), "so does Menu");
    expect(presses(s, FakeKeys{{KEY_LEFT_SHIFT, KEY_F10}, {KEY_F10}}, Action::Select),
           "and Shift+F10");
    expect(!presses(s, FakeKeys{{KEY_F10}, {KEY_F10}}, Action::Select), "a bare F10 does not");
    expect(!presses(s, FakeKeys{{KEY_LEFT_SHIFT, KEY_TAB}, {KEY_TAB}}, Action::Select),
           "Shift+Tab is Guide's chord and does not open the menu");
    expect(presses(s, FakeKeys{{KEY_LEFT_SHIFT, KEY_TAB}, {KEY_TAB}}, Action::Guide),
           "Shift+Tab is Guide");
    expect(presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_TAB}, {KEY_TAB}}, Action::QuickMenu) &&
               !presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_TAB}, {KEY_TAB}}, Action::Select),
           "Ctrl+Tab is the quick menu, not the menu");
    expect(presses(s, FakeKeys{{KEY_ENTER}, {KEY_ENTER}}, Action::Confirm), "Enter confirms");
    expect(presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_F}, {KEY_F}}, Action::Search) &&
               !presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_F}, {KEY_F}}, Action::X),
           "Ctrl+F is the search, not X");
    expect(presses(s, FakeKeys{{KEY_F}, {KEY_F}}, Action::X), "a bare F is X");
    expect(presses(s, FakeKeys{{KEY_Q}, {KEY_Q}}, Action::Quit), "Q quits");
    expect(presses(s, FakeKeys{{KEY_W}, {KEY_W}}, Action::Up), "W is up");
    expect(!presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_UP}, {KEY_UP}}, Action::Up) &&
               presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_UP}, {KEY_UP}}, Action::VolumeUp),
           "Ctrl+Up is the volume, not up");
    expect(presses(s, FakeKeys{{KEY_LEFT_CONTROL, KEY_M}, {KEY_M}}, Action::VolumeMute),
           "Ctrl+M mutes");
}

void everyActionHasAHandlerNameAndKey() {
    const Shortcuts s;
    std::set<std::string> spellings;
    for (const Action action : allActions) {
        expect(!label(action).empty() && actionOf(spelling(action)) == action,
               "an action has a label and a spelling that finds it");
        spellings.insert(std::string{spelling(action)});
        expect(s.primary(action).has_value(), "an action has a key");
        expect(buttonOf(action).has_value() ==
                   !(action == Action::VolumeUp || action == Action::VolumeDown ||
                     action == Action::VolumeMute || action == Action::Quit ||
                     action == Action::QuickMenu),
               "an action is a pad button or one of the shell's own");
    }
    expect(spellings.size() == allActions.size(), "spellings are distinct");
    expect(!actionOf("nothing"), "an unknown spelling is none");
    for (const KeyBinding& binding : s.bindings()) {
        expect(keyName(binding.combo.key).has_value(), "every shipped key can be named");
    }
}

void remapReplacesTheFirstBinding() {
    Shortcuts s;
    expect(s.rebind(Action::Quit, Combo{KEY_F9}).empty(), "a free key is taken");
    expect(s.primary(Action::Quit) == Combo{KEY_F9} && !s.actionFor(Combo{KEY_Q}),
           "the old key is gone");
    expect(s.overrides().keys.size() == 1, "one override is kept");
    expect(s.rebind(Action::Quit, Combo{KEY_Q}).empty() && s.overrides().keys.empty(),
           "choosing the default again keeps no override");
    expect(s.rebind(Action::Up, Combo{KEY_I}).empty() && s.primary(Action::Up) == Combo{KEY_I} &&
               s.actionFor(Combo{KEY_W}) == Action::Up,
           "an action's other keys stay");
    expect(s.keyLabelFor(Button::Up) == "I", "the prompts follow");
    s.reset(Action::Up);
    expect(s.primary(Action::Up) == Combo{KEY_UP} && s.overrides().keys.empty(), "reset");
}

void conflictsAreRefused() {
    Shortcuts s;
    const std::string taken = s.rebind(Action::VolumeMute, Combo{KEY_E});
    expect(taken.find("already Options") != std::string::npos, "naming who has the key");
    expect(s.primary(Action::VolumeMute) == Combo{KEY_M, true, false}, "and nothing changed");
    expect(!s.rebind(Action::Quit, Combo{KEY_LEFT_SHIFT}).empty(), "a modifier alone is refused");
    expect(!s.rebind(Action::Quit, Combo{KEY_LEFT_ALT}).empty(), "so is a key no cap names");
    expect(s.rebind(Action::VolumeMute, Combo{KEY_E, true, false}).empty(),
           "the same key with Ctrl is another combination");
    expect(s.rebind(Action::VolumeMute, Combo{KEY_E, true, false}).empty(), "again is fine");
}

void padChordsCanBeRemapped() {
    Shortcuts s;
    expect(s.padChord(Action::VolumeUp) == PadChord{Button::L2, Button::Up}, "the default chord");
    expect(!s.padChord(Action::Confirm), "a button action has none");
    expect(!s.rebind(Action::Confirm, PadChord{Button::L2, Button::A}).empty(),
           "a button action takes no chord");
    expect(!s.rebind(Action::VolumeUp, PadChord{Button::A, Button::B}).empty(),
           "the modifier must be one that does nothing alone");
    expect(!s.rebind(Action::VolumeUp, PadChord{Button::L2, Button::L2}).empty(), "not itself");
    expect(!s.rebind(Action::VolumeUp, PadChord{Button::L2, Button::Down}).empty(),
           "another action's chord is refused");
    expect(s.rebind(Action::VolumeUp, PadChord{Button::R2, Button::Up}).empty() &&
               s.padChord(Action::VolumeUp) == PadChord{Button::R2, Button::Up},
           "a free chord is taken");
    expect(describe(need(s.padChord(Action::VolumeUp), "a chord")) == "R2 + Up", "its name");
    s.resetAll();
    expect(s.overrides() == ShortcutOverrides{}, "reset puts back the defaults");
}

void quickMenuHasItsDefaults() {
    const Shortcuts s;
    expect(s.padChord(Action::QuickMenu) == PadChord{Button::Guide, Button::A},
           "Guide + A is the quick menu on a pad");
    expect(s.primary(Action::QuickMenu) == Combo{KEY_TAB, true, false}, "Ctrl+Tab on a keyboard");
    expect(worksInGame(Action::QuickMenu) && takesPadChord(Action::QuickMenu),
           "it works over a game and takes a chord");
    Shortcuts changed;
    expect(changed.rebind(Action::QuickMenu, Combo{KEY_F9}).empty() &&
               changed.overrides().keys.at(Action::QuickMenu) == Combo{KEY_F9},
           "it can be remapped like any action");
    expect(!changed.rebind(Action::Guide, Combo{KEY_F9}).empty(), "and holds its key");
}

void combosHaveNames() {
    expect(describe(Combo{KEY_TAB, false, true}) == "Shift+Tab", "a modified cap");
    expect(describe(Combo{KEY_F5, true, true}) == "Ctrl+Shift+F5", "both modifiers");
    expect(comboNamed("ctrl+Up") == Combo{KEY_UP, true, false}, "a spelled combination");
    expect(comboNamed("m") == Combo{KEY_M}, "a letter");
    expect(!comboNamed("ctrl+nothing") && !comboNamed(""), "unknown names");
    for (const int key : bindableKeys()) {
        const std::string name = need(keyName(key), "a key name");
        expect(comboNamed(name) == Combo{key}, "every bindable key round-trips its name");
    }
}

Event press(Button button, bool pressed = true) {
    Event event;
    event.button = button;
    event.pressed = pressed;
    return event;
}

void guideIsATapOrAModifier() {
    const Shortcuts s;
    PadChords chords{s};
    auto fed = chords.feed({press(Button::Guide)});
    expect(fed.events.empty() && fed.actions.empty(), "Guide waits for its release");
    fed = chords.feed({press(Button::Guide, false)});
    expect(fed.events.size() == 2 && fed.events[0].button == Button::Guide && fed.events[0].pressed &&
               fed.events[1].button == Button::Guide && !fed.events[1].pressed,
           "a tap comes out as a press and a release");
    fed = chords.feed({press(Button::Guide)});
    fed = chords.feed({press(Button::A)});
    expect(fed.events.empty() && fed.actions == std::vector{Action::QuickMenu},
           "Guide with A is the quick menu and no A press");
    fed = chords.feed({press(Button::A, false), press(Button::Guide, false)});
    expect(fed.events.empty() && fed.actions.empty(), "its release is swallowed with the chord");
    fed = chords.feed({press(Button::Guide), press(Button::Up), press(Button::Up, false)});
    expect(fed.events.size() == 2 && fed.actions.empty(), "Guide with a non-chord button passes it");
    fed = chords.feed({press(Button::Guide, false)});
    expect(fed.events.size() == 2 && fed.events[0].button == Button::Guide,
           "and Guide is still a tap on release");
}

void chordsAreToldFromPresses() {
    const Shortcuts s;
    PadChords chords{s};
    auto fed = chords.feed({press(Button::Up), press(Button::Up, false)});
    expect(fed.events.size() == 2 && fed.actions.empty(), "Up alone is Up");
    fed = chords.feed({press(Button::L2)});
    expect(fed.events.size() == 1 && fed.actions.empty(), "the modifier alone passes through");
    fed = chords.feed({press(Button::Up), press(Button::Up, false)});
    expect(fed.events.empty() && fed.actions == std::vector{Action::VolumeUp},
           "Up with L2 held is the volume and no button press");
    fed = chords.feed({press(Button::Left)});
    expect(fed.actions == std::vector{Action::VolumeMute}, "Left is mute");
    fed = chords.feed({press(Button::Left, false), press(Button::A), press(Button::A, false)});
    expect(fed.actions.empty() && fed.events.size() == 2,
           "a button that is no chord with L2 passes, its swallowed twin released silently");
    fed = chords.feed({press(Button::L2, false), press(Button::Up)});
    expect(fed.events.size() == 2 && fed.events[1].button == Button::Up && fed.actions.empty(),
           "with L2 released Up is Up again");
    fed = chords.feed({press(Button::Up, false)});
    expect(fed.events.size() == 1, "and its release passes");

    Event connected;
    connected.kind = Event::Kind::Connected;
    fed = chords.feed({connected});
    expect(fed.events.size() == 1, "other events pass through");
}

} // namespace

int main() {
    promptsNameTheBoundKeys();
    shippedKeysAreUnchanged();
    everyActionHasAHandlerNameAndKey();
    remapReplacesTheFirstBinding();
    conflictsAreRefused();
    padChordsCanBeRemapped();
    combosHaveNames();
    chordsAreToldFromPresses();
    guideIsATapOrAModifier();
    quickMenuHasItsDefaults();
    std::printf("shortcuts: all checks passed\n");
    return 0;
}

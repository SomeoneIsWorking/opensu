// The chord rule both the shell's controls and the game's virtual pad follow.
#include "chord_gate.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

#include "check.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::gamepad::Button;
using opensu::gamepad::Chord;
using opensu::gamepad::ChordGate;
using opensu::gamepad::ChordTriggers;
using opensu::test::expect;
using Step = ChordGate::Step;
using Steps = std::vector<Step>;

constexpr ChordGate::Clock::time_point start{};

Step edge(Button button, bool pressed) {
    return {Step::Kind::Edge, button, pressed, Chord{}};
}

Step played(Button modifier, Button trigger) {
    return {Step::Kind::Chord, Button::None, false, Chord{modifier, trigger}};
}

Step longHold(Button modifier) {
    return {Step::Kind::LongHold, modifier, true, Chord{}};
}

Step longHoldEnded(Button modifier) {
    return {Step::Kind::LongHoldEnded, modifier, false, Chord{}};
}

ChordGate gate() {
    ChordGate made;
    made.setTriggers(ChordTriggers{{Chord{Button::Guide, Button::A},
                                    Chord{Button::Guide, Button::B}, Chord{Button::L2, Button::Up}},
                                   500ms});
    return made;
}

void plainButtonsPass() {
    ChordGate g = gate();
    expect(g.feed(Button::A, true, start) == Steps{edge(Button::A, true)}, "A alone is A");
    expect(g.feed(Button::A, false, start) == Steps{edge(Button::A, false)}, "and its release");
    expect(g.feed(Button::R3, true, start) == Steps{edge(Button::R3, true)},
           "a button no bound chord starts with is never held back");
    expect(g.passes(Button::A) && g.passes(Button::R3), "they pass");
}

void aChordSwallowsItsButtons() {
    ChordGate g = gate();
    expect(g.feed(Button::Guide, true, start).empty() && !g.passes(Button::Guide),
           "a modifier is held back");
    expect(g.feed(Button::A, true, start) == Steps{played(Button::Guide, Button::A)},
           "a bound trigger is the chord");
    expect(!g.passes(Button::A), "and is kept back while held");
    expect(g.feed(Button::A, false, start).empty(), "its release is swallowed");
    expect(g.feed(Button::B, true, start) == Steps{played(Button::Guide, Button::B)},
           "the modifier still held plays another chord");
    expect(g.feed(Button::B, false, start).empty() && g.feed(Button::Guide, false, start).empty(),
           "and the modifier's release is swallowed after a chord");
    expect(g.passes(Button::A) && g.passes(Button::B), "the triggers pass again");
}

void aLoneModifierIsATapUnderTheHoldDelay() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::L2, true, start));
    expect(g.feed(Button::L2, false, start + 499ms) ==
               Steps{edge(Button::L2, true), edge(Button::L2, false)},
           "released under the hold delay it is a tap");
    static_cast<void>(g.feed(Button::L2, true, start + 1s));
    expect(g.feed(Button::L2, false, start + 1s + 500ms).empty(),
           "released at the hold delay or later it is nothing");
}

void anotherButtonLetsTheModifierOut() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::L2, true, start));
    expect(g.feed(Button::A, true, start) == Steps{edge(Button::L2, true), edge(Button::A, true)},
           "a button that is no chord with it lets the modifier out first");
    expect(g.passes(Button::L2), "the modifier passes from then on");
    expect(g.feed(Button::Up, true, start) == Steps{edge(Button::Up, true)},
           "and starts no chord until it is released");
    expect(g.feed(Button::L2, false, start + 2s) == Steps{edge(Button::L2, false)},
           "its release passes however long it was held");
    expect(!g.passes(Button::L2), "a modifier not held is kept at rest");
}

void modifiersStack() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::L2, true, start));
    expect(g.feed(Button::Guide, true, start) == Steps{edge(Button::L2, true)},
           "a second modifier that is no chord with the first lets the first out");
    expect(g.feed(Button::A, true, start) == Steps{played(Button::Guide, Button::A)},
           "and waits for its own chord");
}

void anyButtonCanBeAModifier() {
    ChordGate g;
    g.setTriggers(ChordTriggers{{Chord{Button::Start, Button::Y}}, 500ms});
    expect(g.feed(Button::Start, true, start).empty(), "bound as a modifier, Start waits");
    expect(g.feed(Button::Start, false, start + 100ms) ==
               Steps{edge(Button::Start, true), edge(Button::Start, false)},
           "a Start tap is still Start");
    static_cast<void>(g.feed(Button::Start, true, start + 1s));
    expect(g.feed(Button::Y, true, start + 1s) == Steps{played(Button::Start, Button::Y)},
           "Start held + Y is the chord");
}

void aLongHoldIsReportedOnce() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::Guide, true, start));
    expect(g.tick(start + 499ms).empty(), "no long hold under the hold delay");
    expect(g.tick(start + 500ms) == Steps{longHold(Button::Guide)},
           "a modifier held alone for the hold delay is a long hold");
    expect(g.tick(start + 2s).empty(), "reported once");
    expect(g.feed(Button::Guide, false, start + 2s) == Steps{longHoldEnded(Button::Guide)},
           "its release ends the long hold and is no tap");
    expect(g.tick(start + 3s).empty(), "nothing held, nothing reported");
}

void aChordEndsTheLongHold() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::Guide, true, start));
    static_cast<void>(g.tick(start + 600ms));
    expect(g.feed(Button::A, true, start + 700ms) ==
               Steps{longHoldEnded(Button::Guide), played(Button::Guide, Button::A)},
           "a chord played during a long hold ends it, then plays");
    expect(g.tick(start + 2s).empty(), "the modifier still held is no long hold again");
    expect(g.feed(Button::A, false, start + 2s).empty() &&
               g.feed(Button::Guide, false, start + 2s).empty(),
           "and the releases are swallowed as after any chord");
}

void aLetOutEndsTheLongHold() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::L2, true, start));
    expect(g.tick(start + 500ms) == Steps{longHold(Button::L2)}, "L2 held is a long hold too");
    expect(g.feed(Button::X, true, start + 600ms) ==
               Steps{longHoldEnded(Button::L2), edge(Button::L2, true), edge(Button::X, true)},
           "a button that is no chord ends it and lets the modifier out");
    expect(g.tick(start + 2s).empty(), "a let-out modifier is no long hold");
}

void aQuickChordIsNoLongHold() {
    ChordGate g = gate();
    static_cast<void>(g.feed(Button::Guide, true, start));
    static_cast<void>(g.feed(Button::A, true, start + 100ms));
    expect(g.tick(start + 1s).empty(), "a chord under the hold delay shows nothing");
}

} // namespace

int main() {
    plainButtonsPass();
    aChordSwallowsItsButtons();
    aLoneModifierIsATapUnderTheHoldDelay();
    anotherButtonLetsTheModifierOut();
    modifiersStack();
    anyButtonCanBeAModifier();
    aLongHoldIsReportedOnce();
    aChordEndsTheLongHold();
    aLetOutEndsTheLongHold();
    aQuickChordIsNoLongHold();
    std::printf("chord_gate: all checks passed\n");
    return 0;
}

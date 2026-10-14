// One physical pad's events as the virtual Xbox 360 pad and opensu's controls see them.
#include "pad_translator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include <linux/input-event-codes.h>

#include "check.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::gamepad::Button;
using opensu::gamepad::Chord;
using opensu::gamepad::ChordGate;
using opensu::gamepad::ChordTriggers;
using opensu::gamepad::Event;
using opensu::gamepad::ForwardMap;
using opensu::gamepad::PadCapabilities;
using opensu::gamepad::PadEvent;
using opensu::gamepad::PadTranslator;
using opensu::test::expect;

using Events = std::vector<PadEvent>;

/// A DualSense through hid-playstation: 0..255 sticks and triggers, a hat d-pad.
PadCapabilities dualSense() {
    PadCapabilities caps;
    caps.keys = {BTN_SOUTH, BTN_EAST,   BTN_NORTH, BTN_WEST, BTN_TL,     BTN_TR,    BTN_TL2,
                 BTN_TR2,   BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};
    for (const auto axis : {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ}) {
        caps.axes[static_cast<std::uint16_t>(axis)] = {0, 255};
    }
    caps.axes[ABS_HAT0X] = {-1, 1};
    caps.axes[ABS_HAT0Y] = {-1, 1};
    return caps;
}

/// A Switch Pro through hid-nintendo: d-pad buttons and digital triggers.
PadCapabilities switchPro() {
    PadCapabilities caps;
    caps.keys = {BTN_SOUTH, BTN_EAST,    BTN_NORTH,     BTN_WEST,      BTN_TL,
                 BTN_TR,    BTN_TL2,     BTN_TR2,       BTN_SELECT,    BTN_START,
                 BTN_MODE,  BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT};
    for (const auto axis : {ABS_X, ABS_Y, ABS_RX, ABS_RY}) {
        caps.axes[static_cast<std::uint16_t>(axis)] = {-32767, 32767};
    }
    return caps;
}

struct Run {
    Events forward;
    std::vector<Event> controls;
};

constexpr ChordGate::Clock::time_point start{};

Run feed(PadTranslator& pad, const Events& in, ChordGate::Clock::time_point at = start) {
    Run run;
    for (const auto& event : in) {
        pad.translate(event, run.forward, run.controls, at);
    }
    return run;
}

/// Whether the virtual pad was given `code` at a value other than rest.
bool moved(const Run& run, std::uint16_t type, std::uint16_t code) {
    return std::ranges::any_of(run.forward, [type, code](const PadEvent& event) {
        return event.type == type && event.code == code && event.value != 0;
    });
}

/// Chords on a digital modifier (Guide) and an analogue one (L2).
ChordTriggers shippedChords() {
    return ChordTriggers{{Chord{Button::Guide, Button::A}, Chord{Button::L2, Button::Up},
                          Chord{Button::L2, Button::Down}, Chord{Button::L2, Button::Left}},
                         500ms};
}

bool reports(const Run& run, Button button, bool pressed) {
    for (const auto& event : run.controls) {
        if (event.kind == Event::Kind::Button && event.button == button &&
            event.pressed == pressed) {
            return true;
        }
    }
    return false;
}

void faceButtonsPassThrough() {
    PadTranslator pad{dualSense()};
    const auto run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}, {EV_SYN, SYN_REPORT, 0}});
    expect(run.forward == Events{{EV_KEY, BTN_SOUTH, 1}, {EV_SYN, SYN_REPORT, 0}},
           "A is forwarded with its report");
    expect(reports(run, Button::A, true), "A is reported to opensu");
    const auto repeat = feed(pad, {{EV_KEY, BTN_SOUTH, 2}});
    expect(repeat.forward.empty() && repeat.controls.empty(), "autorepeat is dropped");
}

void guideStaysWithOpensu() {
    PadTranslator pad{dualSense()};
    const auto run = feed(pad, {{EV_KEY, BTN_MODE, 1}, {EV_KEY, BTN_MODE, 0}});
    expect(run.forward.empty(), "Guide never reaches the game");
    expect(reports(run, Button::Guide, true) && reports(run, Button::Guide, false),
           "Guide presses and releases are reported");
}

void axesRescale() {
    PadTranslator pad{dualSense()};
    const auto run = feed(pad, {{EV_ABS, ABS_X, 0}, {EV_ABS, ABS_RY, 255}, {EV_ABS, ABS_Z, 255}});
    expect(run.forward ==
               Events{{EV_ABS, ABS_X, -32768}, {EV_ABS, ABS_RY, 32767}, {EV_ABS, ABS_Z, 255}},
           "sticks span the virtual pad's 16-bit range, triggers its 8-bit one");
    expect(reports(run, Button::Left, true), "the left stick held left is Left");
    const auto centre = feed(pad, {{EV_ABS, ABS_X, 128}});
    expect(reports(centre, Button::Left, false), "centring the stick releases Left");
}

void dpadButtonsBecomeHat() {
    PadTranslator pad{switchPro()};
    auto run = feed(pad, {{EV_KEY, BTN_DPAD_UP, 1}});
    expect(run.forward == Events{{EV_ABS, ABS_HAT0Y, -1}}, "d-pad up is the hat up");
    expect(reports(run, Button::Up, true), "and Up for opensu");
    run = feed(pad, {{EV_KEY, BTN_DPAD_DOWN, 1}, {EV_KEY, BTN_DPAD_UP, 0}});
    expect(run.forward == Events{{EV_ABS, ABS_HAT0Y, 0}, {EV_ABS, ABS_HAT0Y, 1}},
           "up and down together cancel, then down alone is down");
}

void digitalTriggersDriveAxes() {
    PadTranslator pad{switchPro()};
    const auto run = feed(pad, {{EV_KEY, BTN_TR2, 1}});
    expect(run.forward == Events{{EV_ABS, ABS_RZ, 255}}, "ZR pulls the right trigger fully");
    expect(reports(run, Button::R2, true), "and is R2 for opensu");
}

void chordsStayFromTheGame() {
    PadTranslator pad{switchPro()};
    pad.setChords(shippedChords());
    Run run = feed(pad, {{EV_KEY, BTN_MODE, 1}, {EV_KEY, BTN_SOUTH, 1}});
    expect(run.forward.empty(), "Guide + A sends no A to the game");
    expect(reports(run, Button::A, true), "opensu still hears the edges, to play the chord");
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_MODE, 0}}, start + 50ms);
    expect(run.forward.empty(), "nor its release");

    run = feed(pad, {{EV_KEY, BTN_TL2, 1}, {EV_KEY, BTN_DPAD_UP, 1}});
    expect(!moved(run, EV_ABS, ABS_Z) && !moved(run, EV_ABS, ABS_HAT0Y),
           "L2 + Up sends neither L2 nor Up");
    run = feed(pad, {{EV_KEY, BTN_DPAD_UP, 0}, {EV_KEY, BTN_TL2, 0}}, start + 50ms);
    expect(run.forward.empty(), "nor their releases");

    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}});
    expect(run.forward == Events{{EV_KEY, BTN_SOUTH, 1}},
           "a button no chord starts with is at once");
    feed(pad, {{EV_KEY, BTN_SOUTH, 0}});
}

void aLoneModifierIsATapOrNothing() {
    PadTranslator pad{switchPro()};
    pad.setChords(shippedChords());
    Run run = feed(pad, {{EV_KEY, BTN_TL2, 1}});
    expect(run.forward.empty(), "L2 waits to see whether a chord follows");
    run = feed(pad, {{EV_KEY, BTN_TL2, 0}}, start + 120ms);
    expect(run.forward == Events{{EV_ABS, ABS_Z, 255}, {EV_SYN, SYN_REPORT, 0}, {EV_ABS, ABS_Z, 0}},
           "released under the hold delay it is a tap, in two reports");
    feed(pad, {{EV_KEY, BTN_TL2, 1}}, start + 1s);
    run = feed(pad, {{EV_KEY, BTN_TL2, 0}}, start + 1s + 500ms);
    expect(run.forward.empty(), "held for the hold delay and released alone it is nothing");

    feed(pad, {{EV_KEY, BTN_TL2, 1}}, start + 2s);
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}}, start + 2s + 10ms);
    expect(run.forward == Events{{EV_ABS, ABS_Z, 255}, {EV_KEY, BTN_SOUTH, 1}},
           "a button that is no chord with it lets L2 out first, then itself");
    run = feed(pad, {{EV_KEY, BTN_DPAD_UP, 1}}, start + 2s + 20ms);
    expect(run.forward == Events{{EV_ABS, ABS_HAT0Y, -1}}, "once let out, L2 starts no chord");
    run = feed(pad, {{EV_KEY, BTN_TL2, 0}}, start + 3s);
    expect(run.forward == Events{{EV_ABS, ABS_Z, 0}}, "and its release reaches the game");
}

void startIsAModifierWhenBound() {
    PadTranslator pad{switchPro()};
    pad.setChords(ChordTriggers{{Chord{Button::Start, Button::Y}}, 500ms});
    Run run = feed(pad, {{EV_KEY, BTN_START, 1}});
    expect(run.forward.empty(), "Start waits while it may start a chord");
    run = feed(pad, {{EV_KEY, BTN_START, 0}}, start + 80ms);
    expect(run.forward ==
               Events{{EV_KEY, BTN_START, 1}, {EV_SYN, SYN_REPORT, 0}, {EV_KEY, BTN_START, 0}},
           "a Start tap still reaches the game as Start");
    run = feed(pad, {{EV_KEY, BTN_START, 1}, {EV_KEY, BTN_WEST, 1}}, start + 1s);
    expect(run.forward.empty() && reports(run, Button::Y, true),
           "Start held + Y is the chord, and neither reaches the game");
    run = feed(pad, {{EV_KEY, BTN_WEST, 0}, {EV_KEY, BTN_START, 0}}, start + 1s + 50ms);
    expect(run.forward.empty(), "nor do their releases");
    feed(pad, {{EV_KEY, BTN_START, 1}}, start + 2s);
    run = feed(pad, {{EV_KEY, BTN_START, 0}}, start + 2s + 600ms);
    expect(run.forward.empty(), "Start held past the hold delay with no partner is nothing");
}

void analogueTriggersPressWithHysteresis() {
    PadTranslator pad{dualSense()};
    Run run = feed(pad, {{EV_ABS, ABS_Z, 100}});
    expect(run.forward == Events{{EV_ABS, ABS_Z, 100}} && run.controls.empty(),
           "a light pull moves the trigger and presses nothing");
    run = feed(pad, {{EV_ABS, ABS_Z, 140}});
    expect(reports(run, Button::L2, true), "past half its travel it is L2");
    run = feed(pad, {{EV_ABS, ABS_Z, 90}});
    expect(run.controls.empty() && run.forward == Events{{EV_ABS, ABS_Z, 90}},
           "easing off a little keeps it pressed");
    run = feed(pad, {{EV_ABS, ABS_Z, 70}});
    expect(reports(run, Button::L2, false), "under three tenths it is released");
    run = feed(pad, {{EV_ABS, ABS_RZ, 255}});
    expect(reports(run, Button::R2, true), "the right trigger is R2");
}

void analogueTriggersAreModifiers() {
    PadTranslator pad{dualSense()};
    pad.setChords(shippedChords());
    Run run = feed(pad, {{EV_ABS, ABS_Z, 100}, {EV_ABS, ABS_Z, 200}});
    expect(run.forward.empty() && reports(run, Button::L2, true),
           "a modifier trigger is kept from the game while it may start a chord");
    run = feed(pad, {{EV_ABS, ABS_HAT0Y, -1}, {EV_ABS, ABS_HAT0Y, 0}, {EV_ABS, ABS_Z, 0}});
    expect(run.forward.empty() && reports(run, Button::Up, true),
           "an analogue L2 + Up is a chord the game never sees");
    feed(pad, {{EV_ABS, ABS_Z, 200}});
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}, {EV_ABS, ABS_Z, 180}});
    expect(run.forward ==
               Events{{EV_ABS, ABS_Z, 200}, {EV_KEY, BTN_SOUTH, 1}, {EV_ABS, ABS_Z, 180}},
           "let out by another button, the trigger reaches the game where it stands");
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 0}, {EV_ABS, ABS_Z, 0}}, start + 2s);
    expect(run.forward == Events{{EV_KEY, BTN_SOUTH, 0}, {EV_ABS, ABS_Z, 0}}, "and follows it");
}

void restAndResume() {
    PadTranslator pad{dualSense()};
    feed(pad, {{EV_KEY, BTN_EAST, 1},
               {EV_ABS, ABS_HAT0X, 1},
               {EV_KEY, BTN_SOUTH, 1},
               {EV_KEY, BTN_SOUTH, 0}});
    expect(pad.rest() ==
               Events{{EV_KEY, BTN_EAST, 0}, {EV_ABS, ABS_HAT0X, 0}, {EV_SYN, SYN_REPORT, 0}},
           "rest releases only what is held");
    expect(pad.resume() == Events{{EV_ABS, ABS_HAT0X, 1}, {EV_SYN, SYN_REPORT, 0}},
           "resume restores held axes and leaves held keys up");
}

void unknownCodesDropped() {
    PadTranslator pad{dualSense()};
    const auto run = feed(pad, {{EV_MSC, MSC_SCAN, 4}, {EV_ABS, ABS_MT_POSITION_X, 3}});
    expect(run.forward.empty() && run.controls.empty(), "scan codes and touchpad stay behind");
}

void aMappedButtonSendsItsOutput() {
    PadTranslator pad{switchPro()};
    pad.setForwardMap(ForwardMap{{{Button::A, {Button::B}}, {Button::B, {}}}, {}});
    Run run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}});
    expect(run.forward == Events{{EV_KEY, BTN_EAST, 1}}, "A mapped to B reaches the game as B");
    expect(reports(run, Button::A, true) && !reports(run, Button::B, true),
           "while opensu's own controls still hear A");
    run = feed(pad, {{EV_KEY, BTN_EAST, 1}});
    expect(run.forward.empty() && reports(run, Button::B, true),
           "B mapped to nothing sends nothing, and opensu still hears B");
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 0}});
    expect(run.forward == Events{{EV_KEY, BTN_EAST, 0}}, "A's release releases B");
    feed(pad, {{EV_KEY, BTN_EAST, 0}});

    pad.setForwardMap(ForwardMap{{{Button::Up, {Button::A}}, {Button::L2, {Button::R2}}}, {}});
    run = feed(pad, {{EV_KEY, BTN_DPAD_UP, 1}, {EV_KEY, BTN_TL2, 1}});
    expect(run.forward == Events{{EV_KEY, BTN_SOUTH, 1}, {EV_ABS, ABS_RZ, 255}},
           "d-pad Up can send A and L2 can send R2, with no hat and no L2");
    expect(reports(run, Button::Up, true) && reports(run, Button::L2, true),
           "and opensu hears Up and L2");
    feed(pad, {{EV_KEY, BTN_DPAD_UP, 0}, {EV_KEY, BTN_TL2, 0}});

    pad.setForwardMap({});
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}});
    expect(run.forward == Events{{EV_KEY, BTN_SOUTH, 1}}, "with no map A is A again");
}

void aMappedTriggerKeepsItsTravel() {
    PadTranslator pad{dualSense()};
    pad.setForwardMap(ForwardMap{{{Button::L2, {Button::R2}}, {Button::R2, {Button::L2}}}, {}});
    const Run run = feed(pad, {{EV_ABS, ABS_Z, 100}, {EV_ABS, ABS_RZ, 255}});
    expect(run.forward == Events{{EV_ABS, ABS_RZ, 100}, {EV_ABS, ABS_Z, 255}},
           "swapped triggers carry their travel to the other side");
}

void guideIsNeverMapped() {
    PadTranslator pad{dualSense()};
    pad.setForwardMap(ForwardMap{{{Button::Guide, {Button::A}}}, {}});
    const Run run = feed(pad, {{EV_KEY, BTN_MODE, 1}, {EV_KEY, BTN_MODE, 0}});
    expect(run.forward.empty() && reports(run, Button::Guide, true),
           "Guide sends nothing to the game whatever the map says");
}

void aGameChordHoldsItsButtons() {
    PadTranslator pad{switchPro()};
    pad.setChords(shippedChords());
    pad.setForwardMap(
        ForwardMap{{}, {{Chord{Button::Select, Button::R1}, {Button::Select, Button::Start}}}});
    Run run = feed(pad, {{EV_KEY, BTN_SELECT, 1}});
    expect(run.forward.empty(), "Select waits: a game chord starts with it");
    run = feed(pad, {{EV_KEY, BTN_TR, 1}}, start + 50ms);
    expect(run.forward == Events{{EV_KEY, BTN_SELECT, 1}, {EV_KEY, BTN_START, 1}},
           "Select + R1 holds Select and Start on the virtual pad, and no R1");
    expect(reports(run, Button::R1, true), "opensu hears every edge");
    run = feed(pad, {{EV_KEY, BTN_TR, 0}}, start + 100ms);
    expect(run.forward == Events{{EV_KEY, BTN_SELECT, 0}, {EV_KEY, BTN_START, 0}},
           "releasing R1 releases them");
    run = feed(pad, {{EV_KEY, BTN_SELECT, 0}}, start + 150ms);
    expect(run.forward.empty(), "and Select's own release is swallowed with the chord");
    run = feed(pad, {{EV_KEY, BTN_MODE, 1}, {EV_KEY, BTN_SOUTH, 1}}, start + 1s);
    expect(run.forward.empty(), "the shell's chords stay kept from the game beside the game's");
    feed(pad, {{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_MODE, 0}}, start + 1s);
}

void aMappedStartTapSendsItsOutput() {
    // Start sends X and starts a game chord: a tap is Start's mapped press, X.
    PadTranslator pad{switchPro()};
    pad.setChords(ChordTriggers{{}, 500ms});
    pad.setForwardMap(ForwardMap{{{Button::Start, {Button::X}}},
                                 {{Chord{Button::Start, Button::A}, {Button::Select}}}});
    feed(pad, {{EV_KEY, BTN_START, 1}});
    Run run = feed(pad, {{EV_KEY, BTN_START, 0}}, start + 80ms);
    expect(run.forward ==
               Events{{EV_KEY, BTN_NORTH, 1}, {EV_SYN, SYN_REPORT, 0}, {EV_KEY, BTN_NORTH, 0}},
           "a Start tap reaches the game as X, in two reports, and never as Start");
    feed(pad, {{EV_KEY, BTN_START, 1}}, start + 1s);
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}}, start + 1s + 50ms);
    expect(run.forward == Events{{EV_KEY, BTN_SELECT, 1}}, "Start + A sends the chord's Select");
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_START, 0}}, start + 1s + 100ms);
    expect(run.forward == Events{{EV_KEY, BTN_SELECT, 0}}, "and nothing else on release");
    feed(pad, {{EV_KEY, BTN_START, 1}}, start + 2s);
    run = feed(pad, {{EV_KEY, BTN_SOUTH, 1}}, start + 2s + 600ms);
    expect(run.forward == Events{{EV_KEY, BTN_SELECT, 1}}, "a chord still plays after a long hold");
    feed(pad, {{EV_KEY, BTN_SOUTH, 0}, {EV_KEY, BTN_START, 0}}, start + 3s);
    feed(pad, {{EV_KEY, BTN_START, 1}}, start + 4s);
    run = feed(pad, {{EV_KEY, BTN_START, 0}}, start + 4s + 600ms);
    expect(run.forward.empty(), "Start held long and released alone sends nothing");
}

} // namespace

int main() {
    faceButtonsPassThrough();
    guideStaysWithOpensu();
    axesRescale();
    dpadButtonsBecomeHat();
    digitalTriggersDriveAxes();
    chordsStayFromTheGame();
    aLoneModifierIsATapOrNothing();
    startIsAModifierWhenBound();
    analogueTriggersPressWithHysteresis();
    analogueTriggersAreModifiers();
    restAndResume();
    unknownCodesDropped();
    aMappedButtonSendsItsOutput();
    aMappedTriggerKeepsItsTravel();
    guideIsNeverMapped();
    aGameChordHoldsItsButtons();
    aMappedStartTapSendsItsOutput();
    std::printf("pad_translator: all checks passed\n");
    return 0;
}

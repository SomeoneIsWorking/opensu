// One physical pad's events as the virtual Xbox 360 pad and iideck's controls see them.
#include "pad_translator.hpp"

#include <cstdio>
#include <vector>

#include <linux/input-event-codes.h>

#include "check.hpp"

namespace {

using iideck::gamepad::Button;
using iideck::gamepad::Event;
using iideck::gamepad::PadCapabilities;
using iideck::gamepad::PadEvent;
using iideck::gamepad::PadTranslator;
using iideck::test::expect;

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

Run feed(PadTranslator& pad, const Events& in) {
    Run run;
    for (const auto& event : in) {
        pad.translate(event, run.forward, run.controls);
    }
    return run;
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
    expect(reports(run, Button::A, true), "A is reported to iideck");
    const auto repeat = feed(pad, {{EV_KEY, BTN_SOUTH, 2}});
    expect(repeat.forward.empty() && repeat.controls.empty(), "autorepeat is dropped");
}

void guideStaysWithIideck() {
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
    expect(reports(run, Button::Up, true), "and Up for iideck");
    run = feed(pad, {{EV_KEY, BTN_DPAD_DOWN, 1}, {EV_KEY, BTN_DPAD_UP, 0}});
    expect(run.forward == Events{{EV_ABS, ABS_HAT0Y, 0}, {EV_ABS, ABS_HAT0Y, 1}},
           "up and down together cancel, then down alone is down");
}

void digitalTriggersDriveAxes() {
    PadTranslator pad{switchPro()};
    const auto run = feed(pad, {{EV_KEY, BTN_TR2, 1}});
    expect(run.forward == Events{{EV_ABS, ABS_RZ, 255}}, "ZR pulls the right trigger fully");
    expect(reports(run, Button::R2, true), "and is R2 for iideck");
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

} // namespace

int main() {
    faceButtonsPassThrough();
    guideStaysWithIideck();
    axesRescale();
    dpadButtonsBecomeHat();
    digitalTriggersDriveAxes();
    restAndResume();
    unknownCodesDropped();
    std::printf("pad_translator: all checks passed\n");
    return 0;
}

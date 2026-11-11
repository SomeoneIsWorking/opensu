// A Wii Remote through hid-wiimote as an ordinary pad: alone held sideways, with its Nunchuk held
// upright, as the translator and opensu's controls then see it; its camera's dots as where it
// points.
#include "wii_remote_layout.hpp"

#include <chrono>
#include <cstdio>
#include <optional>
#include <vector>

#include <linux/input-event-codes.h>

#include "check.hpp"
#include "pad_translator.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::gamepad::ChordGate;
using opensu::gamepad::Event;
using opensu::gamepad::PadCapabilities;
using opensu::gamepad::PadEvent;
using opensu::gamepad::PadTranslator;
using opensu::gamepad::WiiAim;
using opensu::gamepad::WiiPointer;
using opensu::gamepad::WiiRemoteLayout;
using opensu::gamepad::WiiRemotePart;
using opensu::gamepad::wiiRemotePartOf;
using opensu::test::expect;

/// hid-wiimote's Nunchuk device: the stick at +-120, C and Z, the accelerometer.
PadCapabilities nunchuk() {
    PadCapabilities caps;
    caps.keys = {BTN_C, BTN_Z};
    caps.axes[ABS_HAT0X] = {-120, 120};
    caps.axes[ABS_HAT0Y] = {-120, 120};
    for (const auto axis : {ABS_RX, ABS_RY, ABS_RZ}) {
        caps.axes[static_cast<std::uint16_t>(axis)] = {-500, 500};
    }
    return caps;
}

/// The controls `layout`'s pad gives for `in` from `part`.
std::vector<Event> controlsOf(const WiiRemoteLayout& layout, WiiRemotePart part, PadEvent in) {
    PadTranslator translator{layout.capabilities()};
    std::vector<PadEvent> forward;
    std::vector<Event> controls;
    if (const std::optional<PadEvent> mapped = layout.map(part, in)) {
        translator.translate(*mapped, forward, controls, ChordGate::Clock::time_point{});
    }
    return controls;
}

bool presses(const std::vector<Event>& controls, Button button) {
    return controls.size() == 1 && controls.front().button == button && controls.front().pressed;
}

void testParts() {
    expect(wiiRemotePartOf(0x057e, 0x0306, "Nintendo Wii Remote") == WiiRemotePart::Remote,
           "a remote's buttons are its pad");
    expect(wiiRemotePartOf(0x057e, 0x0330, "Nintendo Wii Remote Nunchuk") == WiiRemotePart::Nunchuk,
           "a Motion Plus remote's Nunchuk is part of it");
    expect(!wiiRemotePartOf(0x057e, 0x0306, "Nintendo Wii Remote Accelerometer"),
           "the accelerometer is not read");
    expect(!wiiRemotePartOf(0x045e, 0x028e, "Nintendo Wii Remote"), "the name alone is not enough");
}

void testSideways() {
    const WiiRemoteLayout layout;
    expect(!layout.withNunchuk(), "alone, the remote is held sideways");
    const auto press = [&layout](std::uint16_t code) {
        return controlsOf(layout, WiiRemotePart::Remote, {EV_KEY, code, 1});
    };
    expect(presses(press(BTN_2), Button::A) && presses(press(BTN_1), Button::B),
           "2 and 1 are A and B");
    expect(presses(press(KEY_UP), Button::Left) && presses(press(KEY_RIGHT), Button::Up) &&
               presses(press(KEY_DOWN), Button::Right) && presses(press(KEY_LEFT), Button::Down),
           "the d-pad is turned a quarter");
    expect(presses(press(KEY_NEXT), Button::Start) &&
               presses(press(KEY_PREVIOUS), Button::Select) &&
               presses(press(BTN_MODE), Button::Guide),
           "plus, minus and Home are Start, Select and Guide");
    expect(!layout.map(WiiRemotePart::Nunchuk, {EV_KEY, BTN_C, 1}),
           "a Nunchuk the pad does not hold is not read");
}

void testUpright() {
    const WiiRemoteLayout layout{nunchuk()};
    expect(layout.withNunchuk() && layout.capabilities().isGamepad(),
           "with its Nunchuk the remote is a pad with a stick");
    const auto press = [&layout](WiiRemotePart part, std::uint16_t code) {
        return controlsOf(layout, part, {EV_KEY, code, 1});
    };
    expect(presses(press(WiiRemotePart::Remote, BTN_A), Button::A) &&
               presses(press(WiiRemotePart::Remote, BTN_B), Button::B),
           "upright, A and B are A and B");
    expect(presses(press(WiiRemotePart::Remote, KEY_UP), Button::Up), "the d-pad is not turned");
    expect(presses(press(WiiRemotePart::Nunchuk, BTN_C), Button::L1) &&
               presses(press(WiiRemotePart::Nunchuk, BTN_Z), Button::L2),
           "C and Z are the left shoulder and trigger");
    expect(layout.map(WiiRemotePart::Nunchuk, {EV_ABS, ABS_HAT0Y, 120}) ==
               PadEvent{EV_ABS, ABS_Y, -120},
           "the stick pushed up is the left stick up");
    expect(presses(controlsOf(layout, WiiRemotePart::Nunchuk, {EV_ABS, ABS_HAT0X, 120}),
                   Button::Right),
           "and pushed right is Right");
    expect(!layout.map(WiiRemotePart::Nunchuk, {EV_ABS, ABS_RX, 300}),
           "the accelerometer is not the right stick");
}

void testPointer() {
    expect(wiiRemotePartOf(0x057e, 0x0306, "Nintendo Wii Remote IR") == WiiRemotePart::Camera,
           "the camera is the remote's pointer");
    WiiPointer pointer;
    const PadEvent report{EV_SYN, SYN_REPORT, 0};
    expect(!pointer.feed(report), "no dots seen and none before: nothing changed");
    for (const PadEvent dot :
         {PadEvent{EV_ABS, ABS_HAT0X, 312}, PadEvent{EV_ABS, ABS_HAT0Y, 384},
          PadEvent{EV_ABS, ABS_HAT1X, 512}, PadEvent{EV_ABS, ABS_HAT1Y, 384}}) {
        expect(!pointer.feed(dot), "a dot alone is no report");
    }
    const std::optional<WiiAim> aim = pointer.feed(report);
    expect(aim && aim->onScreen && aim->x > 0.59f && aim->x < 0.60f && aim->y > 0.49f &&
               aim->y < 0.51f,
           "two dots left of centre: the remote points right of centre, mirrored");
    expect(!pointer.feed(report), "the same aim again is not reported");
    expect(pointer.press(true), "A while it points is the pointer's button");
    for (const PadEvent gone :
         {PadEvent{EV_ABS, ABS_HAT0X, 1023}, PadEvent{EV_ABS, ABS_HAT0Y, 1023},
          PadEvent{EV_ABS, ABS_HAT1X, 1023}, PadEvent{EV_ABS, ABS_HAT1Y, 1023}}) {
        static_cast<void>(pointer.feed(gone));
    }
    const std::optional<WiiAim> away = pointer.feed(report);
    expect(away && !away->onScreen, "no dot seen: it points off the screen");
    expect(pointer.press(false), "the press is let go though it points away");
    expect(!pointer.press(true) && !pointer.press(false), "pointed away, A is the pad's");
}

} // namespace

int main() try {
    testParts();
    testSideways();
    testUpright();
    testPointer();
    std::printf("wii_remote_layout: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

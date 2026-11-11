// A Wii Remote through hid-wiimote as an ordinary pad: alone held sideways, with its Nunchuk held
// upright, with a Classic Controller read as it, as the translator and opensu's controls then see
// it; its camera's dots as where it points, in the range fitted from two of the screen's corners.
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
using opensu::gamepad::SensorBarPlace;
using opensu::gamepad::WiiAim;
using opensu::gamepad::WiiPointer;
using opensu::gamepad::WiiPointerRange;
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
    expect(!layout.extension(), "alone, the remote is held sideways");
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
    const WiiRemoteLayout layout{WiiRemotePart::Nunchuk, nunchuk()};
    expect(layout.extension() == WiiRemotePart::Nunchuk && layout.capabilities().isGamepad(),
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

/// hid-wiimote's Classic Controller device: both sticks and the triggers' travel at +-30.
PadCapabilities classic() {
    PadCapabilities caps;
    caps.keys = {BTN_A,    BTN_B,        BTN_X,    BTN_Y,  BTN_TL,   BTN_TR,   BTN_TL2,  BTN_TR2,
                 KEY_NEXT, KEY_PREVIOUS, BTN_MODE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};
    for (std::uint16_t axis = ABS_HAT1X; axis <= ABS_HAT3Y; ++axis) {
        caps.axes[axis] = {-30, 30};
    }
    return caps;
}

void testClassic() {
    expect(wiiRemotePartOf(0x057e, 0x0306, "Nintendo Wii Remote Classic Controller") ==
               WiiRemotePart::Classic,
           "the Classic Controller is part of its remote");
    const WiiRemoteLayout layout{WiiRemotePart::Classic, classic()};
    expect(layout.capabilities().isGamepad(), "with a Classic Controller the remote is a pad");
    const auto press = [&layout](std::uint16_t code) {
        return controlsOf(layout, WiiRemotePart::Classic, {EV_KEY, code, 1});
    };
    expect(presses(press(BTN_B), Button::A) && presses(press(BTN_A), Button::B) &&
               presses(press(BTN_Y), Button::X) && presses(press(BTN_X), Button::Y),
           "its buttons by place: B south, A east, Y west, X north");
    expect(presses(press(BTN_TL2), Button::L1) && presses(press(BTN_TR2), Button::R1) &&
               presses(press(BTN_TL), Button::L2) && presses(press(BTN_TR), Button::R2),
           "ZL and ZR are the shoulders, L and R the triggers");
    expect(presses(press(KEY_UP), Button::Up) && presses(press(KEY_NEXT), Button::Start) &&
               presses(press(BTN_MODE), Button::Guide),
           "its d-pad, plus and Home");
    expect(layout.map(WiiRemotePart::Classic, {EV_ABS, ABS_HAT1Y, 30}) ==
               PadEvent{EV_ABS, ABS_Y, -30},
           "the left stick up is the left stick up");
    expect(layout.map(WiiRemotePart::Classic, {EV_ABS, ABS_HAT2X, 30}) ==
               PadEvent{EV_ABS, ABS_RX, 30},
           "the right stick is the right stick");
    expect(!layout.map(WiiRemotePart::Classic, {EV_ABS, ABS_HAT3X, 40}),
           "the triggers' travel is not read");
    expect(!layout.map(WiiRemotePart::Remote, {EV_KEY, BTN_A, 1}),
           "the remote hanging off it is not read");
}

void testRange() {
    // The camera's aims at the corners, the bar above the screen: the screen is the lower part.
    const std::optional<WiiPointerRange> range =
        WiiPointerRange::fromCorners(0.2f, 0.6f, 0.8f, 0.9f);
    const auto near = [](float value, float wanted) {
        return value > wanted - 0.001f && value < wanted + 0.001f;
    };
    expect(range && near(range->across(0.5f), 0.5f) && near(range->down(0.75f), 0.5f),
           "the corners' aims are the screen's edges");
    expect(range->barPlace() == SensorBarPlace::Above, "the camera's middle is above the screen");
    expect(WiiPointerRange::fromCorners(0.2f, 0.1f, 0.8f, 0.4f)->barPlace() ==
               SensorBarPlace::Below,
           "and below it, the other way");
    expect(near(WiiPointerRange::fromCorners(0.8f, 0.9f, 0.2f, 0.3f)->across(0.2f), 1.0f),
           "a camera that reads across the other way is fitted too");
    expect(!WiiPointerRange::fromCorners(0.5f, 0.5f, 0.51f, 0.9f),
           "one spot pointed at twice fits nothing");

    WiiPointer pointer;
    pointer.setRange(*range);
    const PadEvent report{EV_SYN, SYN_REPORT, 0};
    // The bar's two ends 200 dots apart about 512 across, 576 down: the camera's 0.5, 0.75.
    for (const PadEvent dot :
         {PadEvent{EV_ABS, ABS_HAT0X, 412}, PadEvent{EV_ABS, ABS_HAT0Y, 575},
          PadEvent{EV_ABS, ABS_HAT1X, 611}, PadEvent{EV_ABS, ABS_HAT1Y, 575}}) {
        static_cast<void>(pointer.feed(dot));
    }
    const std::optional<WiiAim> middle = pointer.feed(report);
    expect(middle && middle->onScreen && middle->x > 0.49f && middle->x < 0.51f &&
               middle->y > 0.49f && middle->y < 0.51f,
           "the screen's middle, in the range");
    for (const PadEvent dot :
         {PadEvent{EV_ABS, ABS_HAT0Y, 200}, PadEvent{EV_ABS, ABS_HAT1Y, 200}}) {
        static_cast<void>(pointer.feed(dot));
    }
    const std::optional<WiiAim> above = pointer.feed(report);
    expect(above && !above->onScreen, "past the top edge it points off the screen");
}

} // namespace

int main() try {
    testParts();
    testSideways();
    testUpright();
    testPointer();
    testClassic();
    testRange();
    std::printf("wii_remote_layout: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// A Wii Remote pointing at the screen as the shell's pointer: the pointer follows it, its pointer
// button presses where it points and lets go there though it points away, pointing away the
// pointer leaves, and its other buttons are the pad's.
#include "pointing_pads.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::app::PointingPads;
using opensu::gamepad::Button;
using opensu::gamepad::Event;
using opensu::test::expect;

constexpr const char* remote = "/dev/input/event9";

Event aim(bool onScreen, float x = 0.0f, float y = 0.0f) {
    return Event{.kind = Event::Kind::Pointer,
                 .device = "Nintendo Wii Remote",
                 .source = remote,
                 .onScreen = onScreen,
                 .x = x,
                 .y = y};
}

Event button(bool pressed) {
    return Event{.kind = Event::Kind::PointerButton,
                 .pressed = pressed,
                 .device = "Nintendo Wii Remote",
                 .source = remote};
}

} // namespace

int main() try {
    std::vector<std::string> calls;
    PointingPads pads{PointingPads::Hooks{
        [&calls](float x, float y) {
            calls.push_back("move " + std::to_string(x) + " " + std::to_string(y));
        },
        [&calls](float x, float y, bool down) {
            calls.push_back(std::string{down ? "down " : "up "} + std::to_string(x) + " " +
                            std::to_string(y));
        },
        [&calls] {
            calls.emplace_back("leave");
        }}};
    expect(pads.take(aim(true, 0.25f, 0.5f)) && calls.back() == "move 0.250000 0.500000",
           "pointing at the screen moves the pointer there");
    expect(pads.take(button(true)) && calls.back() == "down 0.250000 0.500000",
           "the pointer button presses there");
    expect(pads.take(aim(false)) && calls.back() == "leave", "pointing away, the pointer leaves");
    expect(pads.take(button(false)) && calls.back() == "up 0.250000 0.500000",
           "the press is let go where it was");
    expect(!pads.take(Event{.kind = Event::Kind::Button,
                            .button = Button::A,
                            .pressed = true,
                            .device = "Nintendo Wii Remote",
                            .source = remote}),
           "the pad's buttons are the pad's");
    std::printf("pointing_pads: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

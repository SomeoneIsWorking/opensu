// A Wii Remote pointing at the screen as the shell's pointer: the pointer follows it, its A is a
// press where it points, released even once it points away, and its A pointed away is the pad's.
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
    return Event{
        .kind = Event::Kind::Pointer, .source = remote, .onScreen = onScreen, .x = x, .y = y};
}

Event a(bool pressed, const char* source = remote) {
    return Event{
        .kind = Event::Kind::Button, .button = Button::A, .pressed = pressed, .source = source};
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
        }}};
    expect(!pads.take(a(true)) && !pads.take(a(false)), "a pad that never pointed keeps its A");
    expect(pads.take(aim(true, 0.25f, 0.5f)) && calls.back() == "move 0.250000 0.500000",
           "pointing at the screen moves the pointer there");
    expect(pads.take(a(true)) && calls.back() == "down 0.250000 0.500000",
           "A while it points presses there");
    expect(!pads.take(a(true, "/dev/input/event3")), "another pad's A stays that pad's");
    expect(pads.take(aim(false)) && pads.take(a(false)) && calls.back() == "up 0.250000 0.500000",
           "the press is let go where it was, though the remote points away");
    expect(!pads.take(a(true)), "pointed away, its A is the pad's A again");
    expect(
        !pads.take(Event{
            .kind = Event::Kind::Button, .button = Button::B, .pressed = true, .source = remote}),
        "B is always the pad's");
    std::printf("pointing_pads: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

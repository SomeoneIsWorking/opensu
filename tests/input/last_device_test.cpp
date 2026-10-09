// The last-used device: pad buttons and keys and moving or clicking pointers switch it; nothing
// else does.
#include "input/last_device.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::gamepad::Event;
using opensu::input::Device;
using opensu::input::LastDevice;
using opensu::test::expect;

Event event(Event::Kind kind, Button button, bool pressed) {
    return Event{.kind = kind, .button = button, .pressed = pressed, .device = {}};
}

Event press(Button button) {
    return event(Event::Kind::Button, button, true);
}

void startsOnThePad() {
    expect(LastDevice{}.current() == Device::Pad, "the shell starts with the controller's prompts");
}

void switchesOnRealInput() {
    LastDevice device;
    device.noteKey();
    expect(device.current() == Device::KeyboardMouse, "a key switches to the keyboard");
    device.notePad(press(Button::A));
    expect(device.current() == Device::Pad, "the next pad button switches back at once");
    device.notePointer(3.0f, 0.0f, false);
    expect(device.current() == Device::KeyboardMouse, "a moving pointer switches to the keyboard");
    device.notePad(event(Event::Kind::Button, Button::B, false));
    expect(device.current() == Device::Pad, "a pad button release is input too");
    device.notePointer(0.0f, 0.0f, true);
    expect(device.current() == Device::KeyboardMouse, "a click switches to the keyboard");
}

void ignoresNoise() {
    LastDevice device;
    device.noteKey();
    device.notePointer(0.0f, 0.0f, false);
    expect(device.current() == Device::KeyboardMouse, "a still pointer is not pad input");
    device.notePad(event(Event::Kind::Connected, Button::None, false));
    device.notePad(event(Event::Kind::Disconnected, Button::None, false));
    expect(device.current() == Device::KeyboardMouse, "a pad connecting or leaving is not input");
    device.notePad(event(Event::Kind::Button, Button::None, true));
    expect(device.current() == Device::KeyboardMouse, "an event with no button is not input");
    LastDevice onPad;
    onPad.notePointer(0.0f, 0.0f, false);
    expect(onPad.current() == Device::Pad, "a still pointer does not take the prompts from a pad");
}

} // namespace

int main() {
    startsOnThePad();
    switchesOnRealInput();
    ignoresNoise();
    std::puts("last_device: ok");
    return 0;
}

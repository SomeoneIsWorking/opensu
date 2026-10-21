// The fixed mapping from pads and keys to RETRO_DEVICE_JOYPAD.
#include "joypad.hpp"

#include <cstdio>
#include <string>

#include <SDL3/SDL_scancode.h>
#include <libretro.h>

#include "check.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::gamepad::Event;
using opensu::retro::Joypad;
using opensu::test::expect;

Event press(Button button, bool pressed, const std::string& source) {
    Event event;
    event.kind = Event::Kind::Button;
    event.button = button;
    event.pressed = pressed;
    event.source = source;
    return event;
}

void mapping() {
    expect(Joypad::idOf(Button::A) == RETRO_DEVICE_ID_JOYPAD_B, "the bottom face button is B");
    expect(Joypad::idOf(Button::B) == RETRO_DEVICE_ID_JOYPAD_A, "the right face button is A");
    expect(Joypad::idOf(Button::X) == RETRO_DEVICE_ID_JOYPAD_Y, "the left face button is Y");
    expect(Joypad::idOf(Button::Y) == RETRO_DEVICE_ID_JOYPAD_X, "the top face button is X");
    expect(Joypad::idOf(Button::L1) == RETRO_DEVICE_ID_JOYPAD_L, "L1 is L");
    expect(Joypad::idOf(Button::R2) == RETRO_DEVICE_ID_JOYPAD_R2, "R2 is R2");
    expect(Joypad::idOf(Button::Select) == RETRO_DEVICE_ID_JOYPAD_SELECT, "Select is Select");
    expect(Joypad::idOf(Button::Up) == RETRO_DEVICE_ID_JOYPAD_UP, "Up is Up");
    expect(!Joypad::idOf(Button::Guide), "Guide belongs to the shell");
    expect(Joypad::idOfKey(SDL_SCANCODE_Z) == RETRO_DEVICE_ID_JOYPAD_B, "Z is B");
    expect(Joypad::idOfKey(SDL_SCANCODE_X) == RETRO_DEVICE_ID_JOYPAD_A, "X is A");
    expect(Joypad::idOfKey(SDL_SCANCODE_A) == RETRO_DEVICE_ID_JOYPAD_Y, "A is Y");
    expect(Joypad::idOfKey(SDL_SCANCODE_S) == RETRO_DEVICE_ID_JOYPAD_X, "S is X");
    expect(Joypad::idOfKey(SDL_SCANCODE_Q) == RETRO_DEVICE_ID_JOYPAD_L, "Q is L");
    expect(Joypad::idOfKey(SDL_SCANCODE_W) == RETRO_DEVICE_ID_JOYPAD_R, "W is R");
    expect(Joypad::idOfKey(SDL_SCANCODE_RETURN) == RETRO_DEVICE_ID_JOYPAD_START, "Enter is Start");
    expect(Joypad::idOfKey(SDL_SCANCODE_RSHIFT) == RETRO_DEVICE_ID_JOYPAD_SELECT,
           "Right Shift is Select");
    expect(Joypad::idOfKey(SDL_SCANCODE_LEFT) == RETRO_DEVICE_ID_JOYPAD_LEFT, "Left is Left");
    expect(!Joypad::idOfKey(SDL_SCANCODE_ESCAPE), "Escape drives no button");
}

void state() {
    Joypad joypad;
    joypad.apply(press(Button::A, true, "/dev/input/event3"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_B) == 1, "a held button reads 1");
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_A) == 0, "another button reads 0");
    joypad.key(SDL_SCANCODE_RETURN, true);
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_MASK) ==
               ((1 << RETRO_DEVICE_ID_JOYPAD_B) | (1 << RETRO_DEVICE_ID_JOYPAD_START)),
           "the mask holds pad and keyboard buttons");
    joypad.key(SDL_SCANCODE_RETURN, false);
    joypad.apply(press(Button::A, false, "/dev/input/event3"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_MASK) == 0, "released buttons read 0");
}

void twoSources() {
    Joypad joypad;
    // The hat and the stick report Up separately; the button is up only when both let go.
    joypad.apply(press(Button::Up, true, "pad"));
    joypad.apply(press(Button::Up, true, "pad"));
    joypad.apply(press(Button::Up, false, "pad"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_UP) == 1, "Up stays held by its other source");
    joypad.apply(press(Button::Up, false, "pad"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_UP) == 0, "Up is released by both");
    joypad.apply(press(Button::Start, true, "one"));
    joypad.apply(press(Button::Start, true, "two"));
    joypad.apply(press(Button::Start, false, "one"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_START) == 1, "a second pad keeps Start held");
    Event gone;
    gone.kind = Event::Kind::Disconnected;
    gone.source = "two";
    joypad.apply(gone);
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_START) == 0, "a pad leaving lets its buttons go");
    joypad.apply(press(Button::Guide, true, "one"));
    expect(joypad.state(RETRO_DEVICE_ID_JOYPAD_MASK) == 0, "Guide reaches no core button");
}

void sticks() {
    Joypad joypad;
    joypad.sticks("one", {.leftX = 1000, .leftY = -20000, .rightX = 0, .rightY = 0});
    joypad.sticks("two", {.leftX = -3000, .leftY = 0, .rightX = 0, .rightY = 32767});
    expect(joypad.analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) == -3000,
           "of two pads the stick further out wins");
    expect(joypad.analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) == -20000,
           "the left stick's y");
    expect(joypad.analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) == 32767,
           "the right stick's y");
    expect(joypad.analog(RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2) == 0,
           "analogue buttons read 0");
    Event gone;
    gone.kind = Event::Kind::Disconnected;
    gone.source = "two";
    joypad.apply(gone);
    expect(joypad.analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) == 0,
           "a pad leaving centres its sticks");
}

} // namespace

int main() {
    mapping();
    state();
    twoSources();
    sticks();
    std::printf("joypad: all checks passed\n");
    return 0;
}

// Each console's and genre's touch layout through the touch layer: its buttons, its analog sticks'
// lean, and the setting that picks it.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "gamepad/event.hpp"
#include "settings/video_settings.hpp"
#include "support/test_main.hpp"
#include "touch/shooter_keys.hpp"
#include "touch/touch_layer.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::settings::TouchLayout;
using opensu::touch::ControlPlace;
using opensu::touch::Finger;
using opensu::touch::LayoutWish;
using opensu::touch::TouchLayer;
using opensu::touch::WindowSurface;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "touch_layout: FAILED: %s\n", what);
        std::exit(1);
    }
}

WindowSurface surface(unsigned width, unsigned height, float dp) {
    return WindowSurface{.width = width,
                         .height = height,
                         .safeX = 0.0F,
                         .safeY = 0.0F,
                         .safeWidth = static_cast<float>(width),
                         .safeHeight = static_cast<float>(height),
                         .dp = dp};
}

std::vector<ControlPlace> placesOf(TouchLayer& layer, const LayoutWish& wish,
                                   const WindowSurface& target) {
    layer.setLayout(wish);
    layer.layout(target);
    return layer.places();
}

bool has(const std::vector<ControlPlace>& places, Button button) {
    return std::ranges::find(places, button, &ControlPlace::button) != places.end();
}

std::size_t sticksIn(const std::vector<ControlPlace>& places) {
    return static_cast<std::size_t>(
        std::ranges::count(places, Button::None, &ControlPlace::button));
}

void everyLayoutFits() {
    TouchLayer layer{"no-art"};
    for (const WindowSurface& target : {surface(2400, 1080, 2.75F), surface(1080, 2400, 2.75F)}) {
        for (std::size_t at = 1; at < opensu::settings::touchLayoutChoices.size(); ++at) {
            const auto layout = static_cast<TouchLayout>(at);
            for (const TouchLayout console : {TouchLayout::Pad, TouchLayout::Gamepad}) {
                for (const ControlPlace& place :
                     placesOf(layer, LayoutWish{layout, console, false}, target)) {
                    expect(place.x >= 0.0F && place.y >= 0.0F &&
                               place.x <= static_cast<float>(target.width) &&
                               place.y <= static_cast<float>(target.height),
                           "every layout stays on the screen, wide and upright");
                }
            }
        }
    }
}

void consolesHaveTheirButtons() {
    TouchLayer layer{"no-art"};
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto n64 =
        placesOf(layer, {TouchLayout::Nintendo64, TouchLayout::Nintendo64, false}, phone);
    expect(has(n64, Button::A) && has(n64, Button::X) && has(n64, Button::L2) &&
               has(n64, Button::Start) && !has(n64, Button::Select) && sticksIn(n64) == 3,
           "Nintendo 64: A, B, Z, Start, its stick, d-pad and C buttons");
    const auto digital =
        placesOf(layer, {TouchLayout::PlayStation, TouchLayout::PlayStation, false}, phone);
    expect(has(digital, Button::R2) && !has(digital, Button::L3) && sticksIn(digital) == 1,
           "a digital PlayStation game: four shoulders and the d-pad");
    const auto analog =
        placesOf(layer, {TouchLayout::PlayStation, TouchLayout::PlayStation, true}, phone);
    expect(has(analog, Button::L3) && has(analog, Button::R3) && sticksIn(analog) == 3,
           "an analog one adds both sticks with L3 and R3");
    const auto six = placesOf(layer, {TouchLayout::Genesis6, TouchLayout::Pad, false}, phone);
    expect(has(six, Button::L1) && has(six, Button::Y) && has(six, Button::R1) &&
               has(six, Button::Select),
           "Genesis 6-button: X Y Z and Mode");
    const auto gba =
        placesOf(layer, {TouchLayout::GameBoyAdvance, TouchLayout::GameBoyAdvance, false}, phone);
    expect(has(gba, Button::L1) && !has(gba, Button::X), "Game Boy Advance: B A, L R");
}

void genresTakeTheConsolesButtons() {
    TouchLayer layer{"no-art"};
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto pc = placesOf(layer, {TouchLayout::Racing, TouchLayout::Gamepad, false}, phone);
    expect(has(pc, Button::R2) && has(pc, Button::L2) && has(pc, Button::R1),
           "racing on a gamepad: the triggers are the pedals");
    const auto snes =
        placesOf(layer, {TouchLayout::Racing, TouchLayout::SuperNintendo, false}, phone);
    expect(has(snes, Button::A) && has(snes, Button::X) && has(snes, Button::Left) == false,
           "on a console: the bottom and left faces");
    const auto jump =
        placesOf(layer, {TouchLayout::Platformer, TouchLayout::SuperNintendo, false}, phone);
    expect(has(jump, Button::A) && has(jump, Button::X) && has(jump, Button::Start),
           "a platformer: Jump and Action, with the console's Start");
}

void sticksLean() {
    TouchLayer layer{"no-art"};
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto places = placesOf(layer, {TouchLayout::Gamepad, TouchLayout::Gamepad, false}, phone);
    const auto left = std::ranges::min_element(places, {}, [](const ControlPlace& place) {
        return place.button == Button::None ? place.x : 1e9F;
    });
    const float x = left->x;
    const float y = left->y;
    expect(layer.finger(Finger{1, x, y, Finger::Phase::Down}) == false &&
               layer.sticks() == opensu::gamepad::Sticks{},
           "a finger on the stick's centre leans it not at all");
    expect(layer.finger(Finger{1, x + 600.0F, y, Finger::Phase::Move}), "moving it is a change");
    const opensu::gamepad::Sticks pushed = layer.sticks();
    expect(pushed.leftX == 32767 && pushed.leftY == 0 && layer.held().empty(),
           "past its reach the left stick leans all the way, holding no button");
    layer.finger(Finger{1, x + 600.0F, y, Finger::Phase::Up});
    expect(layer.sticks() == opensu::gamepad::Sticks{}, "let go, it rests");
}

void cButtonsPushTheRightStick() {
    TouchLayer layer{"no-art"};
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto places =
        placesOf(layer, {TouchLayout::Nintendo64, TouchLayout::Nintendo64, false}, phone);
    const auto c = std::ranges::max_element(places, {}, [](const ControlPlace& place) {
        return place.button == Button::None ? place.x : -1.0F;
    });
    layer.finger(Finger{2, c->x, c->y - (c->radius * 0.5F), Finger::Phase::Down});
    expect(layer.sticks().rightY == -32767 && layer.sticks().rightX == 0,
           "C-up pushes the right stick all the way up");
}

void shooterPlaysKeysAndMouse() {
    TouchLayer layer{"no-art"};
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto alone =
        placesOf(layer, {TouchLayout::Shooter, TouchLayout::SuperNintendo, false, false}, phone);
    expect(has(alone, Button::A) && !has(alone, Button::R2) && has(alone, Button::L1),
           "with no keys and mouse to send to, the Shooter is the console's own");
    const auto places =
        placesOf(layer, {TouchLayout::Shooter, TouchLayout::Gamepad, false, true}, phone);
    expect(has(places, Button::R2) && has(places, Button::L2) && has(places, Button::L3) &&
               has(places, Button::Start) && sticksIn(places) == 1,
           "the Shooter: a move stick, Fire, Aim, Sprint and the rest");

    // A finger on the right half that lands on no button turns the view, holding nothing.
    const float x = 1500.0F;
    const float y = 300.0F;
    layer.finger(Finger{3, x, y, Finger::Phase::Down});
    layer.finger(Finger{3, x + 275.0F, y - 55.0F, Finger::Phase::Move});
    const auto [across, down] = layer.takeLook();
    expect(across == 100.0F && down == -20.0F && layer.held().empty(),
           "a swipe of 275 by -55 pixels at 2.75 per dp turns the view 100 by -20 dp");
    expect(layer.takeLook() == std::pair{0.0F, 0.0F}, "the look is taken once");
    layer.finger(Finger{3, x, y, Finger::Phase::Up});

    const std::vector<Button> held{Button::Up, Button::Right, Button::A,    Button::R2,
                                   Button::L2, Button::L3,    Button::Guide};
    const opensu::touch::DeskInput desk = opensu::touch::deskInput(held);
    expect(desk.keys == std::vector<std::uint8_t>{0x1a, 0x07, 0x2c, 0xe1} &&
               desk.mouseButtons == 0b101,
           "W, D, Space and Shift, both mouse buttons; Guide presses nothing");
}

void settingPicksByConsole() {
    using opensu::settings::touchLayoutFor;
    expect(touchLayoutFor("n64") == TouchLayout::Nintendo64 &&
               touchLayoutFor("psx") == TouchLayout::PlayStation &&
               touchLayoutFor("gb") == TouchLayout::TwoButtons &&
               touchLayoutFor("") == TouchLayout::Gamepad &&
               touchLayoutFor("arcade") == TouchLayout::Pad,
           "Auto: the console's own, the gamepad for a game with none");
    opensu::settings::VideoSettings video;
    video.touchLayout = TouchLayout::Racing;
    video.touchConsole = TouchLayout::Nintendo64;
    std::string error;
    const auto read = opensu::settings::parseVideoJson(opensu::settings::videoJson(video), error);
    expect(read && read->touchLayout == TouchLayout::Racing &&
               read->touchConsole == TouchLayout::Nintendo64,
           "the runner is told the layout and the console's own");
}

} // namespace

int main() try {
    everyLayoutFits();
    consolesHaveTheirButtons();
    genresTakeTheConsolesButtons();
    sticksLean();
    cButtonsPushTheRightStick();
    shooterPlaysKeysAndMouse();
    settingPicksByConsole();
    std::printf("touch_layout: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

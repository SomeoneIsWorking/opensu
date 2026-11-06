// An arcade machine's panel from what the cores really send, and laid out through the touch layer.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <libretro.h>

#include "arcade_cabinet.hpp"
#include "check.hpp"
#include "joypad.hpp"
#include "runner_touch.hpp"
#include "support/test_main.hpp"
#include "touch/panel_text.hpp"
#include "touch/touch_layer.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::retro::CabinetListing;
using opensu::retro::cabinetListingOf;
using opensu::retro::InputDescriptor;
using opensu::retro::Joypad;
using opensu::retro::panelOf;
using opensu::retro::RunnerTouch;
using opensu::test::expect;
using opensu::touch::ArcadePanel;
using opensu::touch::Box;
using opensu::touch::ControlPlace;
using opensu::touch::Finger;
using opensu::touch::Limb;
using opensu::touch::TouchLayer;
using opensu::touch::WindowSurface;

constexpr unsigned up = RETRO_DEVICE_ID_JOYPAD_UP;
constexpr unsigned down = RETRO_DEVICE_ID_JOYPAD_DOWN;
constexpr unsigned left = RETRO_DEVICE_ID_JOYPAD_LEFT;
constexpr unsigned right = RETRO_DEVICE_ID_JOYPAD_RIGHT;

std::vector<InputDescriptor> stick() {
    return {{RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin"},
            {RETRO_DEVICE_ID_JOYPAD_START, "Start"},
            {up, "Up"},
            {down, "Down"},
            {left, "Left"},
            {right, "Right"}};
}

std::vector<InputDescriptor> withStick(std::vector<InputDescriptor> buttons) {
    std::vector<InputDescriptor> all = stick();
    all.insert(all.end(), buttons.begin(), buttons.end());
    return all;
}

std::vector<std::string> labels(const ArcadePanel& panel) {
    std::vector<std::string> named;
    named.reserve(panel.buttons.size());
    for (const auto& button : panel.buttons) {
        named.push_back(button.label);
    }
    return named;
}

void namedByTheCore() {
    const ArcadePanel armwar = panelOf(withStick({{RETRO_DEVICE_ID_JOYPAD_B, "Attack"},
                                                  {RETRO_DEVICE_ID_JOYPAD_A, "Jump"},
                                                  {RETRO_DEVICE_ID_JOYPAD_Y, "Shot"}}),
                                       {});
    expect(labels(armwar) == std::vector<std::string>{"Attack", "Jump", "Shot"},
           "FinalBurn Neo's and FB Alpha 2012's names stand");
    expect(armwar.ways == 8 && armwar.buttons[2].button == Button::X,
           "an 8-way stick, and Y is the left face button");

    const ArcadePanel sf2 = panelOf(withStick({{RETRO_DEVICE_ID_JOYPAD_Y, "Weak Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_X, "Medium Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_L, "Strong Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_B, "Weak Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_A, "Medium Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_R, "Strong Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_L2, "Buttons 3x Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_R2, "Buttons 3x Kick"}}),
                                    {});
    expect(sf2.buttons.size() == 6 && sf2.buttons[3].label == "L" &&
               sf2.buttons[3].limb == Limb::Kick,
           "the core's macros are no buttons of the machine");

    const ArcadePanel neogeo = panelOf(withStick({{RETRO_DEVICE_ID_JOYPAD_L3, "Select"},
                                                  {RETRO_DEVICE_ID_JOYPAD_B, "Button A"},
                                                  {RETRO_DEVICE_ID_JOYPAD_A, "Button B"},
                                                  {RETRO_DEVICE_ID_JOYPAD_Y, "Button C"},
                                                  {RETRO_DEVICE_ID_JOYPAD_X, "Button D"},
                                                  {RETRO_DEVICE_ID_JOYPAD_R, "Buttons AB"}}),
                                       {.buttons = 9});
    expect(labels(neogeo) == std::vector<std::string>{"A", "B", "C", "D"},
           "Neo Geo's four, and a listing does not overrule a core's names");

    const ArcadePanel pacman = panelOf(stick(), {.ways = 4});
    expect(pacman.buttons.empty() && pacman.ways == 4, "no buttons, and the listing's 4-way stick");
    const ArcadePanel sideways = panelOf(
        std::vector<InputDescriptor>{
            {left, "Left"}, {right, "Right"}, {RETRO_DEVICE_ID_JOYPAD_B, "Fire"}},
        {});
    expect(sideways.ways == 2 && !sideways.vertical, "a game with left and right alone");
}

void countedByTheListing() {
    const std::vector<InputDescriptor> mame2010 =
        withStick({{RETRO_DEVICE_ID_JOYPAD_L3, "Service/Test"},
                   {RETRO_DEVICE_ID_JOYPAD_R3, "Toggle MAME UI"},
                   {RETRO_DEVICE_ID_JOYPAD_A, "Button 1"},
                   {RETRO_DEVICE_ID_JOYPAD_B, "Button 2"},
                   {RETRO_DEVICE_ID_JOYPAD_X, "Button 3"},
                   {RETRO_DEVICE_ID_JOYPAD_R2, "Turbo Button"},
                   {RETRO_DEVICE_ID_JOYPAD_Y, "Button 4"},
                   {RETRO_DEVICE_ID_JOYPAD_L, "Button 5"},
                   {RETRO_DEVICE_ID_JOYPAD_R, "Button 6"},
                   {RETRO_DEVICE_ID_JOYPAD_L2, "Button 7"}});
    const ArcadePanel three = panelOf(mame2010, {.buttons = 3, .ways = 8});
    expect(labels(three) == std::vector<std::string>{"1", "2", "3"},
           "MAME 2010's table is cut to the buttons the set lists");
    expect(three.buttons[0].button == Button::B, "its Button 1 is libretro's A");
    expect(panelOf(mame2010, {}).buttons.size() == 7, "with no listing, the buttons it reports");
}

void noData() {
    const ArcadePanel none = panelOf({}, {});
    expect(none.ways == 8 && none.buttons.size() == 4, "a stick and four buttons");
    const ArcadePanel listed = panelOf({}, {.buttons = 2, .ways = 4});
    expect(listed.ways == 4 && listed.buttons.size() == 2, "the listing alone serves");
    expect(cabinetListingOf("6:8") == CabinetListing{6, 8, false}, "--arcade's value");
    expect(cabinetListingOf("1:v2") == CabinetListing{1, 2, true}, "a vertical stick's");
    expect(!cabinetListingOf("6") && !cabinetListingOf("a:8"), "anything else is refused");
}

ArcadePanel numbered(std::size_t count) {
    constexpr std::array<Button, 8> order{Button::A,  Button::B,  Button::X,  Button::Y,
                                          Button::L1, Button::R1, Button::L2, Button::R2};
    ArcadePanel panel;
    for (std::size_t at = 0; at < count; ++at) {
        panel.buttons.push_back({order.at(at), std::to_string(at + 1)});
    }
    return panel;
}

WindowSurface surface(unsigned width, unsigned height, float dp) {
    return WindowSurface{.width = width,
                         .height = height,
                         .safeWidth = static_cast<float>(width),
                         .safeHeight = static_cast<float>(height),
                         .dp = dp};
}

/// A 4:3 picture centred on `target`.
Box fourThree(const WindowSurface& target) {
    const float width = static_cast<float>(target.height) * 4.0F / 3.0F;
    return Box{(static_cast<float>(target.width) - width) * 0.5F, 0.0F, width,
               static_cast<float>(target.height)};
}

const ControlPlace& place(const std::vector<ControlPlace>& places, Button button) {
    return *std::ranges::find(places, button, &ControlPlace::button);
}

void rowsOfTheCabinet() {
    TouchLayer layer{"no-art"};
    layer.setPanel(numbered(6));
    const WindowSurface target = surface(1920, 1080, 2.0F);
    layer.layout(target, fourThree(target));
    const std::vector<ControlPlace> six = layer.places();
    expect(place(six, Button::A).x < place(six, Button::B).x &&
               place(six, Button::B).x < place(six, Button::X).x,
           "the first three run left to right");
    expect(place(six, Button::A).y < place(six, Button::Y).y &&
               place(six, Button::A).x == place(six, Button::Y).x,
           "the second three sit below them");

    layer.setPanel(numbered(4));
    layer.layout(target, fourThree(target));
    const std::vector<ControlPlace> four = layer.places();
    expect(place(four, Button::A).x < place(four, Button::B).x &&
               place(four, Button::X).x < place(four, Button::Y).x,
           "Neo Geo's four run in one row");
    expect(std::abs(place(four, Button::Y).y - place(four, Button::A).y) < 50.0F,
           "with no row below");
}

void onTheBlack() {
    TouchLayer layer{"no-art"};
    layer.setPanel(numbered(3));
    // A wide phone: 480 px of black either side of a 4:3 picture.
    const WindowSurface wide = surface(2400, 1080, 2.0F);
    const Box picture = fourThree(wide);
    layer.layout(wide, picture);
    for (const ControlPlace& control : layer.places()) {
        const bool beside = control.x < picture.x || control.x > picture.x + picture.width;
        expect(beside, "every control sits beside the picture");
    }
    const std::vector<ControlPlace> places = layer.places();
    expect(place(places, Button::None).x < picture.x, "the stick left of it");
    expect(place(places, Button::A).x > picture.x + picture.width, "the buttons right of it");

    // A narrow bar: thumbs come first, so the controls keep their size and reach over the picture.
    const WindowSurface tablet = surface(2800, 1840, 2.2F);
    const Box narrow = fourThree(tablet);
    layer.layout(tablet, narrow);
    const std::vector<ControlPlace> edge = layer.places();
    expect(place(edge, Button::None).radius >= 126.0F * tablet.dp, "the stick stays d-pad sized");
    const float step = place(edge, Button::B).x - place(edge, Button::A).x;
    expect(std::abs(step - (92.0F * tablet.dp)) < 1.0F, "the buttons are 13 mm");
    expect(place(edge, Button::A).x < narrow.x + narrow.width, "and over the picture");
    expect(place(edge, Button::X).x > narrow.x + narrow.width, "the last one on the black");
    expect(place(edge, Button::Select).x > narrow.x + narrow.width, "and Coin stays small there");

    // Six that just fit stay on the black, at the generic pad's size.
    layer.setPanel(numbered(6));
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const Box beside = fourThree(phone);
    layer.layout(phone, beside);
    const std::vector<ControlPlace> six = layer.places();
    const float across = place(six, Button::B).x - place(six, Button::A).x - (8.0F * phone.dp);
    expect(place(six, Button::A).x - (across * 0.5F) >= beside.x + beside.width,
           "the first column clears the picture");
    expect(across >= 52.0F * phone.dp, "at a thumb's size");

    // Upright there is no bar, and the buttons give way to the stick.
    layer.setPanel(numbered(4));
    const WindowSurface upright = surface(1080, 2400, 2.75F);
    layer.layout(upright, Box{0.0F, 0.0F, 1080.0F, 810.0F});
    const std::vector<ControlPlace> standing = layer.places();
    const float step2 = place(standing, Button::B).x - place(standing, Button::A).x;
    const ControlPlace& standingStick = place(standing, Button::None);
    const float stickEdge = standingStick.x + (standingStick.radius * 2.0F / 3.0F);
    expect(place(standing, Button::A).x - (step2 * 0.5F) >= stickEdge, "clear of the stick");
}

void severalFingers() {
    TouchLayer layer{"no-art"};
    layer.setPanel(numbered(3));
    const WindowSurface target = surface(2400, 1080, 2.0F);
    layer.layout(target, fourThree(target));
    const std::vector<ControlPlace> places = layer.places();
    const ControlPlace& stickPlace = place(places, Button::None);
    const ControlPlace& first = place(places, Button::A);
    const ControlPlace& third = place(places, Button::X);
    layer.finger({1, stickPlace.x + 60.0F, stickPlace.y - 60.0F, Finger::Phase::Down});
    layer.finger({2, first.x, first.y, Finger::Phase::Down});
    layer.finger({3, third.x, third.y, Finger::Phase::Down});
    expect(layer.pressed(Button::Up) && layer.pressed(Button::Right) && layer.pressed(Button::A) &&
               layer.pressed(Button::X),
           "a diagonal held with two buttons");
    layer.finger({2, 0.0F, 0.0F, Finger::Phase::Up});
    expect(!layer.pressed(Button::A) && layer.pressed(Button::Up), "one finger lifts alone");
    layer.finger({1, stickPlace.x - 60.0F, stickPlace.y, Finger::Phase::Move});
    expect(layer.pressed(Button::Left) && !layer.pressed(Button::Right), "the stick follows");
}

void waysOfTheStick() {
    TouchLayer layer{"no-art"};
    ArcadePanel panel = numbered(1);
    panel.ways = 4;
    layer.setPanel(panel);
    const WindowSurface target = surface(2400, 1080, 2.0F);
    layer.layout(target, fourThree(target));
    const ControlPlace centre = place(layer.places(), Button::None);
    layer.finger({1, centre.x + 60.0F, centre.y - 50.0F, Finger::Phase::Down});
    expect(layer.held().size() == 1, "a 4-way stick has no diagonal");
    layer.releaseAll();
    panel.ways = 2;
    layer.setPanel(panel);
    layer.layout(target, fourThree(target));
    layer.finger({1, centre.x - 10.0F, centre.y - 60.0F, Finger::Phase::Down});
    expect(layer.pressed(Button::Left) && layer.held().size() == 1, "a 2-way stick goes sideways");
}

void thePadStays() {
    TouchLayer layer{"no-art"};
    layer.layout(surface(1280, 800, 1.0F));
    const std::vector<ControlPlace> places = layer.places();
    expect(places.size() == 10, "the pad's d-pad, four faces, L, R, Select, Start and pause");
    expect(place(places, Button::L1).y < 200.0F, "with its shoulder buttons");
}

} // namespace

void fightersByName() {
    const ArcadePanel sf2 = panelOf(withStick({{RETRO_DEVICE_ID_JOYPAD_B, "Weak Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_A, "Medium Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_R, "Strong Kick"},
                                               {RETRO_DEVICE_ID_JOYPAD_Y, "Weak Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_X, "Medium Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_L, "Strong Punch"},
                                               {RETRO_DEVICE_ID_JOYPAD_L2, "Buttons 3x Punch"}}),
                                    {});
    expect(sf2.buttons.size() == 6, "a macro is not on the panel");
    expect(labels(sf2) == std::vector<std::string>{"L", "M", "H", "L", "M", "H"},
           "strengths weakest first");
    expect(sf2.buttons[0].limb == Limb::Punch && sf2.buttons[0].button == Button::X &&
               sf2.buttons[3].limb == Limb::Kick,
           "punches before kicks, each keeping its core's input");

    const ArcadePanel kombat = panelOf(withStick({{RETRO_DEVICE_ID_JOYPAD_B, "High Punch"},
                                                  {RETRO_DEVICE_ID_JOYPAD_A, "Block"},
                                                  {RETRO_DEVICE_ID_JOYPAD_Y, "Low Kick"},
                                                  {RETRO_DEVICE_ID_JOYPAD_X, "Hurricane Kick"},
                                                  {RETRO_DEVICE_ID_JOYPAD_L, "Punch"}}),
                                       {});
    expect(labels(kombat) == std::vector<std::string>{"H", "Block", "L", "Hurricane Kick", ""},
           "other buttons keep their names and their place");
    expect(kombat.buttons[3].limb == Limb::None, "a named move is no strength");
    expect(kombat.buttons[4].limb == Limb::Punch, "a plain punch is a fist alone");

    const ArcadePanel numberedSix = panelOf({}, {.buttons = 6, .ways = 8, .fighter = true});
    expect(labels(numberedSix) == std::vector<std::string>{"L", "M", "H", "L", "M", "H"} &&
               numberedSix.buttons[2].limb == Limb::Punch &&
               numberedSix.buttons[3].limb == Limb::Kick,
           "a fighter's board reads its six as punches then kicks");
    expect(panelOf({}, {.buttons = 6, .ways = 8}).buttons[0].limb == Limb::None,
           "six on any other board stay numbered");
    expect(cabinetListingOf("6:8f") == CabinetListing{6, 8, false, true}, "the launch says so");

    TouchLayer layer{"no-art"};
    layer.setPanel(numberedSix);
    const WindowSurface target = surface(2400, 1080, 2.0F);
    layer.layout(target, fourThree(target));
    const std::vector<ControlPlace> rows = layer.places();
    expect(place(rows, Button::Y).y > place(rows, Button::A).y + 100.0F, "kicks under punches");
}

void sizes() {
    const WindowSurface phone = surface(2400, 1080, 2.75F);
    const auto onScreen = [&phone](TouchLayer& layer, const Box& picture) {
        layer.setSize(1.75F);
        layer.layout(phone, picture);
        for (const ControlPlace& control : layer.places()) {
            expect(control.x >= 0.0F && control.x <= 2400.0F && control.y >= 0.0F &&
                       control.y <= 1080.0F,
                   "the largest size stays on screen");
        }
    };
    TouchLayer pad{"no-art"};
    pad.layout(phone);
    const float faces = place(pad.places(), Button::B).x - place(pad.places(), Button::X).x;
    pad.setSize(0.75F);
    pad.layout(phone);
    const float small = place(pad.places(), Button::B).x - place(pad.places(), Button::X).x;
    expect(std::abs(small - (faces * 0.75F)) < 1.0F, "a smaller size scales the pad");
    onScreen(pad, {});

    TouchLayer panel{"no-art"};
    panel.setPanel(numbered(6));
    onScreen(panel, fourThree(phone));
}

void shortTaps() {
    Joypad joypad{1};
    RunnerTouch touch{"no-art", joypad};
    const CabinetListing listing{3, 8, false};
    touch.cabinet(listing);
    const WindowSurface target = surface(2400, 1080, 2.0F);
    TouchLayer layer{"no-art"};
    layer.setPanel(panelOf({}, listing));
    layer.layout(target);
    const std::vector<ControlPlace> places = layer.places();
    const auto tap = [&](Button button) {
        const ControlPlace& at = place(places, button);
        const std::array<Finger, 2> fingers{
            Finger{.id = 1, .x = at.x, .y = at.y, .phase = Finger::Phase::Down},
            Finger{.id = 1, .x = at.x, .y = at.y, .phase = Finger::Phase::Up}};
        return touch.follow(target, true, fingers);
    };
    const auto held = [&](unsigned id) {
        return joypad.state(0, id) != 0;
    };
    const auto frame = [&] {
        touch.ran();
        touch.follow(target, true, {});
    };

    tap(Button::A);
    touch.follow(target, true, {});
    expect(held(RETRO_DEVICE_ID_JOYPAD_B), "a tap waits for a frame the core runs");
    frame();
    expect(!held(RETRO_DEVICE_ID_JOYPAD_B), "and is one frame long");

    tap(Button::Select);
    for (int shown = 0; shown < 4; ++shown) {
        expect(held(RETRO_DEVICE_ID_JOYPAD_SELECT), "a coin is a few frames long");
        frame();
    }
    expect(!held(RETRO_DEVICE_ID_JOYPAD_SELECT), "and then let go");

    tap(Button::Start);
    frame();
    expect(held(RETRO_DEVICE_ID_JOYPAD_START), "Start outlasts a frame too");
    frame();
    frame();
    frame();
    expect(!held(RETRO_DEVICE_ID_JOYPAD_START), "as long as a coin");

    expect(tap(Button::Guide), "a tap on pause asks for the menu");
}

void uprightUnderThePicture() {
    const WindowSurface tablet = surface(1840, 2800, 2.2F);
    const auto underBand = [&tablet](const TouchLayer& layer) {
        const float top = static_cast<float>(tablet.height) - layer.band();
        expect(layer.band() > 0.0F && layer.band() <= static_cast<float>(tablet.height) / 2.0F,
               "the controls keep a band, at most half the screen");
        for (const ControlPlace& at : layer.places()) {
            expect(at.y >= top && at.y <= static_cast<float>(tablet.height) && at.x >= 0.0F &&
                       at.x <= static_cast<float>(tablet.width),
                   "every control stands in it");
        }
    };

    TouchLayer panel{"no-art"};
    panel.offerRotate();
    panel.setPanel(panelOf({}, CabinetListing{6, 8, false}));
    panel.layout(tablet);
    underBand(panel);
    const std::vector<ControlPlace> places = panel.places();
    const ControlPlace& pause = place(places, Button::Guide);
    const ControlPlace& rotate = place(places, Button::Rotate);
    expect(rotate.y == pause.y && rotate.x > pause.x, "Rotate screen stands beside pause");

    panel.setSize(1.75F);
    panel.layout(tablet);
    underBand(panel);

    TouchLayer pad{"no-art"};
    pad.offerRotate();
    pad.layout(tablet);
    underBand(pad);
    expect(!pad.places().empty(), "the pad stands there too");

    TouchLayer wide{"no-art"};
    wide.setPanel(panelOf({}, CabinetListing{3, 8, false}));
    wide.layout(surface(2800, 1840, 2.2F));
    expect(wide.band() == 0.0F, "a landscape screen keeps no band");
}

void rotatePressedOnce() {
    Joypad joypad{1};
    RunnerTouch touch{"no-art", joypad};
    const WindowSurface target = surface(2400, 1080, 2.0F);
    touch.follow(target, true, {});
    expect(touch.places().find("rotate") != std::string::npos, "the runner's pad offers it");

    TouchLayer layer{"no-art"};
    layer.offerRotate();
    layer.layout(target);
    const std::vector<ControlPlace> places = layer.places();
    const ControlPlace& at = place(places, Button::Rotate);
    const std::array<Finger, 2> tap{
        Finger{.id = 1, .x = at.x, .y = at.y, .phase = Finger::Phase::Down},
        Finger{.id = 1, .x = at.x, .y = at.y, .phase = Finger::Phase::Up}};
    expect(!touch.follow(target, true, tap), "it is no pause");
    expect(touch.takeRotate(), "a tap presses it");
    expect(!touch.takeRotate(), "once");

    TouchLayer remote{"no-art"};
    remote.layout(target);
    for (const ControlPlace& shown : remote.places()) {
        expect(shown.button != Button::Rotate, "a remote game's pad has none");
    }
    remote.setPanel(panelOf({}, CabinetListing{6, 8, false}));
    remote.layout(surface(1920, 1080, 2.0F));
    for (const ControlPlace& shown : remote.places()) {
        expect(shown.button != Button::Rotate, "nor has a remote game's panel");
    }
}

void panelCrossesTheWireAsText() {
    const ArcadePanel panel{
        .ways = 8,
        .vertical = true,
        .buttons = {{.button = Button::A, .label = "Light Punch", .limb = Limb::Punch},
                    {.button = Button::B, .label = "", .limb = Limb::None}}};
    const std::string text = opensu::touch::panelText(panel);
    expect(opensu::touch::panelFromText(text) == panel, "a panel reads back as it was written");
    expect(!opensu::touch::panelFromText("").has_value(), "no text is no panel");
    expect(!opensu::touch::panelFromText("9 0\n").has_value(), "a stick has at most eight ways");
    expect(!opensu::touch::panelFromText("8 0\nnothing 0 A\n").has_value(),
           "a button the pad does not have is refused");
}

int main() try {
    namedByTheCore();
    countedByTheListing();
    noData();
    rowsOfTheCabinet();
    onTheBlack();
    severalFingers();
    waysOfTheStick();
    thePadStays();
    fightersByName();
    sizes();
    shortTaps();
    uprightUnderThePicture();
    rotatePressedOnce();
    panelCrossesTheWireAsText();
    std::printf("arcade_panel: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

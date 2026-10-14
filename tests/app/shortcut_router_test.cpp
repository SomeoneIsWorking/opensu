// The shortcut router: the quick menu action fires from Ctrl+Tab and from Guide + A, a lone Guide
// tap still reaches the shell as Guide, and the chord does not leak its buttons.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "host_fakes.hpp"
#include "shortcut_router.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

gamepad::Event pad(Button button, bool pressed) {
    gamepad::Event event;
    event.kind = gamepad::Event::Kind::Button;
    event.button = button;
    event.pressed = pressed;
    return event;
}

struct Rig {
    Rig()
        : volume{std::make_unique<test::FakeMixer>(), {}},
          router{shortcuts, volume, retro,
                 app::ShortcutRouter::Hooks{.quit =
                                                [] {
                                                },
                                            .quickMenu =
                                                [this] {
                                                    ++quickMenus;
                                                },
                                            .gameRunning =
                                                [this] {
                                                    return gameRunning;
                                                },
                                            .closeGame =
                                                [this] {
                                                    ++closed;
                                                },
                                            .openDevices =
                                                [this] {
                                                    ++devices;
                                                },
                                            .goHome =
                                                [this] {
                                                    ++homes;
                                                },
                                            .say =
                                                [this](const std::string& text, bool error) {
                                                    said.emplace_back(text, error);
                                                }}} {
    }

    input::Shortcuts shortcuts;
    app::VolumeControl volume;
    /// No launch began, so the game has no control channel.
    app::RetroSession retro{"unused"};
    app::ShortcutRouter router;
    int quickMenus{0};
    bool gameRunning{false};
    int closed{0};
    int devices{0};
    int homes{0};
    std::vector<std::pair<std::string, bool>> said;
};

void ctrlTabOpensTheQuickMenu() {
    Rig rig;
    const std::vector<gamepad::Event> events =
        rig.router.fromCombo(input::Combo{input::keys::Tab, true, false});
    expect(rig.quickMenus == 1 && events.empty(), "Ctrl+Tab is the action, not a button");
}

void guidePlusAOpensTheQuickMenu() {
    Rig rig;
    std::vector<gamepad::Event> out = rig.router.fromPad({pad(Button::Guide, true)});
    expect(out.empty(), "a Guide press waits to see if it is a chord");
    out = rig.router.fromPad({pad(Button::A, true)});
    expect(rig.quickMenus == 1 && out.empty(), "Guide + A is the quick menu and sends no A");
    out = rig.router.fromPad({pad(Button::A, false), pad(Button::Guide, false)});
    expect(rig.quickMenus == 1 && out.empty(), "releasing the chord sends nothing either");
}

void aLoneGuideTapIsGuide() {
    Rig rig;
    static_cast<void>(rig.router.fromPad({pad(Button::Guide, true)}));
    const std::vector<gamepad::Event> out = rig.router.fromPad({pad(Button::Guide, false)});
    expect(rig.quickMenus == 0 && out.size() == 2 && out[0].button == Button::Guide &&
               out[0].pressed && !out[1].pressed,
           "a tap comes out as a press and release of Guide on release");
}

void chord(Rig& rig, Button modifier, Button trigger) {
    static_cast<void>(rig.router.fromPad({pad(modifier, true), pad(trigger, true)}));
    static_cast<void>(rig.router.fromPad({pad(trigger, false), pad(modifier, false)}));
}

void gameActionsTakeChords() {
    Rig rig;
    expect(rig.shortcuts.rebind(input::Action::Devices, input::PadChord{Button::Guide, Button::B})
                   .empty() &&
               rig.shortcuts
                   .rebind(input::Action::CloseGame, input::PadChord{Button::Guide, Button::X})
                   .empty() &&
               rig.shortcuts
                   .rebind(input::Action::SaveState, input::PadChord{Button::Guide, Button::Y})
                   .empty() &&
               rig.shortcuts.rebind(input::Action::Home, input::PadChord{Button::R3, Button::A})
                   .empty(),
           "the game actions take chords");
    chord(rig, Button::Guide, Button::B);
    expect(rig.devices == 1, "Guide + B opens Devices");
    chord(rig, Button::R3, Button::A);
    expect(rig.homes == 1, "R3 + A goes home");
    chord(rig, Button::Guide, Button::X);
    expect(rig.closed == 0 &&
               rig.said.back() == std::pair<std::string, bool>{"no game is running", true},
           "Close game with no game says so");
    rig.gameRunning = true;
    chord(rig, Button::Guide, Button::X);
    expect(rig.closed == 1, "with a game it closes it");
    chord(rig, Button::Guide, Button::Y);
    expect(rig.said.back() == std::pair<std::string, bool>{"the game has no control channel", true},
           "Save state on a game not on opensu-retro is refused with a toast");
}

} // namespace

int main() {
    ctrlTabOpensTheQuickMenu();
    guidePlusAOpensTheQuickMenu();
    aLoneGuideTapIsGuide();
    gameActionsTakeChords();
    std::printf("shortcut_router: all checks passed\n");
    return 0;
}

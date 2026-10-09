// The shortcut router: the quick menu action fires from Ctrl+Tab and from Guide + A, a lone Guide
// tap still reaches the shell as Guide, and the chord does not leak its buttons.
#include <cstdio>

#include "host_fakes.hpp"
#include "raylib.h"
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
          router{shortcuts, volume, app::ShortcutRouter::Hooks{[] {
                                                                   },
                                                               [this] {
                                                                   ++quickMenus;
                                                               }}} {
    }

    input::Shortcuts shortcuts;
    app::VolumeControl volume;
    app::ShortcutRouter router;
    int quickMenus{0};
};

void ctrlTabOpensTheQuickMenu() {
    Rig rig;
    const std::vector<gamepad::Event> events = rig.router.fromCombo(input::Combo{KEY_TAB, true, false});
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

} // namespace

int main() {
    ctrlTabOpensTheQuickMenu();
    guidePlusAOpensTheQuickMenu();
    aLoneGuideTapIsGuide();
    std::printf("shortcut_router: all checks passed\n");
    return 0;
}

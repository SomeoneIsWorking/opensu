// The Guide menu's controller: entries follow where the shell is, each entry reaches its hook,
// restart and shut down need a second press, a refusal is said, and B leaves the power list first.
#include <cstdio>

#include "guide_menu_controller.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

struct Rig {
    explicit Rig(bool inGame = false)
        : controller{menu, power, sounds,
                     app::GuideMenuController::Hooks{
                         [inGame] {
                             return ui::GuideContext{inGame, "Roboquest",
                                                     {library::Source::Steam, library::Source::Gog}};
                         },
                         [this](library::Section section) {
                             calls.push_back(section == library::Section::Home ? "home" : "library");
                         },
                         [this](library::Source source) {
                             calls.push_back("store " + std::string{library::label(source)});
                         },
                         [this] {
                             calls.emplace_back("devices");
                         },
                         [this] {
                             calls.emplace_back("settings");
                         },
                         [this] {
                             calls.emplace_back("closeGame");
                         },
                         [this] {
                             calls.emplace_back("quit");
                         },
                         [this](const std::string& text, bool error) {
                             said.push_back((error ? "error: " : "") + text);
                         }}} {
    }

    /// Moves to the entry labelled `label` and presses A.
    void choose(const std::string& label) {
        for (std::size_t index = 0; index < menu.entries().size(); ++index) {
            if (menu.entries()[index].label == label) {
                menu.focusEntry(index);
                controller.act(Button::A);
                return;
            }
        }
        expect(false, ("an entry labelled " + label).c_str());
    }

    ui::GuideMenu menu;
    test::FakePower power;
    audio::SoundPlayer sounds;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    app::GuideMenuController controller;
};

void entriesFollowWhereTheShellIs() {
    Rig home;
    home.controller.open();
    std::vector<std::string> labels;
    for (const ui::GuideEntry& entry : home.menu.entries()) {
        labels.push_back(entry.label);
    }
    expect(labels == std::vector<std::string>{"Home", "Library", "Steam", "GOG", "Devices",
                                              "Settings", "Power"},
           "home lists sections, stores, devices, settings and power");
    Rig game{true};
    game.controller.open();
    expect(game.menu.entries().front().action == ui::GuideAction::Resume &&
               game.menu.entries()[1].action == ui::GuideAction::CloseGame,
           "a running game gets Resume and Close game first");
}

void entriesReachTheirHooks() {
    Rig rig;
    rig.controller.open();
    rig.choose("Library");
    rig.controller.open();
    rig.choose("GOG");
    rig.controller.open();
    rig.choose("Devices");
    rig.controller.open();
    rig.choose("Settings");
    expect(rig.calls == std::vector<std::string>{"library", "store GOG", "devices", "settings"},
           "each entry reaches its hook");
    expect(!rig.menu.isOpen(), "choosing closes the menu");
}

void powerNeedsTwoPressesForRestartAndShutDown() {
    Rig rig;
    rig.controller.open();
    rig.choose("Power");
    expect(rig.menu.inPower() && rig.menu.isOpen(), "Power opens its list");
    rig.choose("Restart");
    expect(rig.power.performed.empty() && rig.menu.armed() && rig.menu.isOpen(),
           "the first press only arms");
    rig.controller.act(Button::A);
    expect(rig.power.performed == std::vector<host::PowerAction>{host::PowerAction::Restart} &&
               !rig.menu.isOpen(),
           "the second press restarts");
    rig.controller.open();
    rig.choose("Power");
    rig.choose("Shut down");
    rig.controller.act(Button::Down);
    expect(!rig.menu.armed(), "moving lets go of an armed entry");
    rig.controller.act(Button::Up);
    rig.controller.act(Button::A);
    expect(rig.power.performed.size() == 1, "an entry that was let go must be armed again");
}

void sleepAndQuitActAtOnceAndRefusalsAreSaid() {
    Rig rig;
    rig.power.refusal = "Access denied";
    rig.controller.open();
    rig.choose("Power");
    rig.choose("Sleep");
    expect(rig.power.performed == std::vector<host::PowerAction>{host::PowerAction::Suspend},
           "sleep acts at once");
    expect(rig.said == std::vector<std::string>{"error: Sleep: Access denied"}, "the refusal is said");
    rig.controller.open();
    rig.choose("Power");
    rig.choose("Quit to desktop");
    expect(rig.calls == std::vector<std::string>{"quit"}, "quit reaches its hook");
}

void backLeavesThePowerListBeforeTheMenu() {
    Rig rig;
    rig.controller.open();
    rig.choose("Power");
    rig.controller.act(Button::B);
    expect(rig.menu.isOpen() && !rig.menu.inPower(), "B returns to the main list");
    rig.controller.act(Button::B);
    expect(!rig.menu.isOpen(), "B closes the menu");
}

void closeGameAndResume() {
    Rig rig{true};
    rig.controller.open();
    rig.choose("Close game");
    rig.controller.open();
    rig.choose("Resume");
    expect(rig.calls == std::vector<std::string>{"closeGame"} && !rig.menu.isOpen(),
           "Close game reaches its hook and Resume only closes");
}

} // namespace

int main() {
    entriesFollowWhereTheShellIs();
    entriesReachTheirHooks();
    powerNeedsTwoPressesForRestartAndShutDown();
    sleepAndQuitActAtOnceAndRefusalsAreSaid();
    backLeavesThePowerListBeforeTheMenu();
    closeGameAndResume();
    std::printf("guide_menu_controller: all checks passed\n");
    return 0;
}

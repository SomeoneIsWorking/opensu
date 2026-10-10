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
    explicit Rig(bool inGame = false, bool session = false, bool installed = false)
        : controller{menu, power, sessionMode, sounds,
                     app::GuideMenuController::Hooks{
                         [inGame, session, installed] {
                             return ui::GuideContext{inGame, "Roboquest", session, installed};
                         },
                         [this](library::Section section) {
                             calls.push_back(section == library::Section::Home ? "home"
                                                                               : "library");
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
                         [this] {
                             calls.emplace_back("install");
                         },
                         [this](const std::string& text, bool error) {
                             said.push_back((error ? "error: " : "") + text);
                         }}} {
    }

    /// Moves to the power button and presses A.
    void openPower() {
        controller.act(Button::Down);
        for (std::size_t i = 0; i < menu.entries().size(); ++i) {
            controller.act(Button::Down);
        }
        expect(menu.powerFocused(), "down reaches the power button");
        controller.act(Button::A);
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
    test::FakeSessionMode sessionMode;
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
    expect(labels == std::vector<std::string>{"Home", "Library", "Devices", "Settings"},
           "home lists sections, devices and settings");
    Rig game{true};
    game.controller.open();
    expect(game.menu.entries().size() == 4, "a running game lists four entries");
    expect(game.menu.entries().front().action == ui::GuideAction::Resume &&
               game.menu.entries()[1].action == ui::GuideAction::CloseGame,
           "a running game gets Resume and Close game first");
}

void entriesReachTheirHooks() {
    Rig rig;
    rig.controller.open();
    rig.choose("Library");
    rig.controller.open();
    rig.choose("Devices");
    rig.controller.open();
    rig.choose("Settings");
    expect(rig.calls == std::vector<std::string>{"library", "devices", "settings"},
           "each entry reaches its hook");
    expect(!rig.menu.isOpen(), "choosing closes the menu");
}

void powerNeedsTwoPressesForRestartAndShutDown() {
    Rig rig;
    rig.controller.open();
    rig.openPower();
    expect(rig.menu.inPower() && rig.menu.isOpen(), "Power opens its list");
    rig.choose("Restart");
    expect(rig.power.performed.empty() && rig.menu.armed() && rig.menu.isOpen(),
           "the first press only arms");
    rig.controller.act(Button::A);
    expect(rig.power.performed == std::vector<host::PowerAction>{host::PowerAction::Restart} &&
               !rig.menu.isOpen(),
           "the second press restarts");
    rig.controller.open();
    rig.openPower();
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
    rig.openPower();
    rig.choose("Sleep");
    expect(rig.power.performed == std::vector<host::PowerAction>{host::PowerAction::Suspend},
           "sleep acts at once");
    expect(rig.said == std::vector<std::string>{"error: Sleep: Access denied"},
           "the refusal is said");
    rig.controller.open();
    rig.openPower();
    rig.choose("Quit to desktop");
    expect(rig.calls == std::vector<std::string>{"quit"}, "quit reaches its hook");
}

void switchToDesktopGoesToSessionMode() {
    Rig rig{false, true};
    rig.controller.open();
    rig.openPower();
    rig.choose("Switch to desktop");
    expect(rig.sessionMode.calls == std::vector<std::string>{"desktop"} &&
               rig.power.performed.empty() && rig.calls.empty() && !rig.menu.isOpen(),
           "Switch to desktop goes to session mode at once");
    rig.sessionMode.refusal = "changes are off in a hidden run";
    rig.controller.open();
    rig.openPower();
    rig.choose("Switch to desktop");
    expect(
        rig.said ==
            std::vector<std::string>{"error: Switch to desktop: changes are off in a hidden run"},
        "a refusal is said");
}

void switchToSessionModeNeedsTwoPresses() {
    Rig rig{false, false, true};
    rig.controller.open();
    rig.openPower();
    rig.choose("Switch to session mode");
    expect(rig.sessionMode.calls.empty() && rig.menu.armed() && rig.menu.isOpen(),
           "the first press only arms");
    expect(rig.menu.shown(rig.menu.focus()) == "Switch to session mode? Press A again",
           "and asks for the second");
    rig.controller.act(Button::A);
    expect(rig.sessionMode.calls == std::vector<std::string>{"session"} && !rig.menu.isOpen(),
           "the second press switches");
    rig.sessionMode.refusal = "sudo: a password is required";
    rig.controller.open();
    rig.openPower();
    rig.choose("Switch to session mode");
    rig.controller.act(Button::A);
    expect(
        rig.said ==
            std::vector<std::string>{"error: Switch to session mode: sudo: a password is required"},
        "a refusal is said");
}

void installSessionModeOpensItsDialogAtOnce() {
    Rig rig;
    rig.controller.open();
    rig.openPower();
    rig.choose("Install session mode");
    expect(rig.calls == std::vector<std::string>{"install"} && !rig.menu.isOpen() &&
               rig.sessionMode.calls.empty(),
           "Install session mode reaches its hook without a second press");
}

void backLeavesThePowerListBeforeTheMenu() {
    Rig rig;
    rig.controller.open();
    rig.openPower();
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
    switchToDesktopGoesToSessionMode();
    switchToSessionModeNeedsTwoPresses();
    installSessionModeOpensItsDialogAtOnce();
    backLeavesThePowerListBeforeTheMenu();
    closeGameAndResume();
    std::printf("guide_menu_controller: all checks passed\n");
    return 0;
}

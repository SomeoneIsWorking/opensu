#include "guide_menu_controller.hpp"

namespace opensu::app {

void GuideMenuController::open() {
    // input-sound.md 3.4 OpenContextMenu: a menu becomes visible (r90.java).
    sounds_.play(audio::Effect::OpenContextMenu);
    menu_.open(hooks_.context());
}

void GuideMenuController::close() {
    if (!menu_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: a menu becomes hidden.
    sounds_.play(audio::Effect::Close);
    menu_.close();
}

void GuideMenuController::toggle() {
    if (menu_.isOpen()) {
        close();
    } else {
        open();
    }
}

void GuideMenuController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (menu_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::B:
        if (menu_.inPower()) {
            sounds_.play(audio::Effect::Navigation);
            menu_.showMain();
        } else {
            close();
        }
        break;
    case gamepad::Button::A:
        choose();
        break;
    default:
        break;
    }
}

void GuideMenuController::choose() {
    // The entry is a reference into the menu, which closing or switching lists rebuilds.
    if (menu_.powerFocused()) {
        sounds_.play(audio::Effect::Navigation);
        menu_.showPower();
        return;
    }
    const ui::GuideAction action = menu_.selected().action;
    switch (action) {
    case ui::GuideAction::Restart:
    case ui::GuideAction::ShutDown:
    case ui::GuideAction::SwitchToSession:
        if (!menu_.armed()) {
            sounds_.play(audio::Effect::Navigation);
            menu_.arm();
            return;
        }
        break;
    default:
        break;
    }
    close();
    switch (action) {
    case ui::GuideAction::Resume:
        break;
    case ui::GuideAction::CloseGame:
        hooks_.closeGame();
        break;
    case ui::GuideAction::Home:
        hooks_.showSection(library::Section::Home);
        break;
    case ui::GuideAction::Library:
        hooks_.showSection(library::Section::Library);
        break;
    case ui::GuideAction::Devices:
        hooks_.openDevices();
        break;
    case ui::GuideAction::Settings:
        hooks_.openSettings();
        break;
    case ui::GuideAction::Sleep:
        perform(host::PowerAction::Suspend);
        break;
    case ui::GuideAction::Restart:
        perform(host::PowerAction::Restart);
        break;
    case ui::GuideAction::ShutDown:
        perform(host::PowerAction::ShutDown);
        break;
    case ui::GuideAction::QuitToDesktop:
        hooks_.quit();
        break;
    case ui::GuideAction::SwitchToDesktop:
        report(session_.switchToDesktop(), "Switch to desktop");
        break;
    case ui::GuideAction::SwitchToSession:
        report(session_.switchToSession(), "Switch to session mode");
        break;
    case ui::GuideAction::InstallSession:
        hooks_.installSession();
        break;
    }
}

void GuideMenuController::perform(host::PowerAction action) {
    report(power_.perform(action), host::label(action));
}

void GuideMenuController::report(const std::string& refused, const char* what) {
    if (!refused.empty()) {
        hooks_.say(std::string{what} + ": " + refused, true);
    }
}

} // namespace opensu::app

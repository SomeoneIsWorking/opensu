#include "context_menu_controller.hpp"

#include <variant>

namespace opensu::app {

void ContextMenuController::open() {
    const library::ShelfItem* focused = hooks_.focused();
    if (focused == nullptr) {
        return;
    }
    target_ = *focused;
    std::string title;
    bool hidden = false;
    if (const auto* game = std::get_if<library::Game>(&*target_)) {
        title = game->title;
        hidden = preferences_.values().hidden.contains(*game);
    } else if (const std::optional<library::Folder> folder = library::folderOf(*target_)) {
        title = library::name(*folder);
    }
    // input-sound.md 3.4 OpenContextMenu: the home context menu becomes visible.
    sounds_.play(audio::Effect::OpenContextMenu);
    menu_.open(std::move(title), ui::contextItemsFor(*target_, hidden));
}

void ContextMenuController::close() {
    sounds_.play(audio::Effect::Close);
    menu_.close();
    target_.reset();
}

void ContextMenuController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (menu_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A: {
        const ui::ContextAction action = menu_.selected();
        menu_.close();
        run(action);
        target_.reset();
        break;
    }
    case gamepad::Button::B:
    case gamepad::Button::Select:
        close();
        break;
    default:
        break;
    }
}

void ContextMenuController::run(ui::ContextAction action) {
    if (!target_) {
        return;
    }
    const auto* game = std::get_if<library::Game>(&*target_);
    switch (action) {
    case ui::ContextAction::Launch:
    case ui::ContextAction::Install:
        hooks_.launch();
        break;
    case ui::ContextAction::Details:
        hooks_.details();
        break;
    case ui::ContextAction::Hide:
    case ui::ContextAction::Unhide:
        if (game != nullptr) {
            hooks_.setHidden(*game, action == ui::ContextAction::Hide);
        }
        break;
    case ui::ContextAction::Open:
        if (const std::optional<library::Folder> folder = library::folderOf(*target_)) {
            hooks_.open(*folder);
        }
        break;
    case ui::ContextAction::Refresh:
        hooks_.refresh();
        break;
    case ui::ContextAction::SignIn:
        if (const auto* launcher = std::get_if<library::Launcher>(&*target_)) {
            hooks_.signIn(launcher->source);
        }
        break;
    }
}

} // namespace opensu::app

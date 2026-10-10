#include "path_editor.hpp"

#include "keyboard_walk.hpp"

namespace opensu::app {

void PathEditor::begin(Request request) {
    request_ = std::move(request);
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    folders_.open(request_.title, request_.start, request_.clearLabel);
}

void PathEditor::cancel() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    entry_.close();
    folders_.close();
}

void PathEditor::choose(const std::filesystem::path& folder) {
    if (const std::string refused = request_.refusal(folder); !refused.empty()) {
        say_(refused, true);
        return;
    }
    cancel();
    request_.done(folder);
}

void PathEditor::openEntry() {
    sounds_.play(audio::Effect::Open);
    entry_.configure("Type a folder path", false);
    entry_.open(folders_.current().string());
}

void PathEditor::closeEntry() {
    sounds_.play(audio::Effect::Close);
    entry_.close();
}

void PathEditor::act(gamepad::Button button) {
    if (entry_.isOpen()) {
        actOnKeys(button);
    } else if (folders_.isOpen()) {
        actInChooser(button);
    }
}

void PathEditor::actInChooser(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (folders_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
        if (folders_.up()) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Right:
    case gamepad::Button::A: {
        const ui::FolderPress press = folders_.press();
        if (press.chosen) {
            choose(*press.chosen);
        } else if (press.type) {
            openEntry();
        } else if (press.clear) {
            cancel();
            request_.done(std::nullopt);
        } else {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    }
    case gamepad::Button::B:
    case gamepad::Button::Start:
        cancel();
        break;
    default:
        break;
    }
}

void PathEditor::actOnKeys(gamepad::Button button) {
    switch (walkKeyboard(entry_, button, sounds_)) {
    case KeyboardOutcome::Confirm:
        confirm();
        break;
    case KeyboardOutcome::Leave:
        closeEntry();
        break;
    case KeyboardOutcome::Handled:
        break;
    }
}

void PathEditor::typeText(std::string_view text) {
    static_cast<void>(entry_.type(text));
}

void PathEditor::backspace() {
    static_cast<void>(entry_.backspace());
}

void PathEditor::confirm() {
    const std::filesystem::path typed{entry_.text()};
    if (const std::string refused = request_.refusal(typed); !refused.empty()) {
        say_(refused, true);
        return;
    }
    cancel();
    request_.done(typed);
}

void PathEditor::dismiss() {
    closeEntry();
}

} // namespace opensu::app

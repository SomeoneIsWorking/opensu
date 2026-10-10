#include "keyboard_walk.hpp"

namespace opensu::app {
ui::Direction directionOf(gamepad::Button button) noexcept {
    switch (button) {
    case gamepad::Button::Up:
        return ui::Direction::Up;
    case gamepad::Button::Down:
        return ui::Direction::Down;
    case gamepad::Button::Left:
        return ui::Direction::Left;
    default:
        return ui::Direction::Right;
    }
}

KeyboardOutcome walkKeyboard(ui::SearchPanel& keyboard, gamepad::Button button,
                             audio::SoundPlayer& sounds) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (keyboard.move(directionOf(button))) {
            sounds.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        return keyboard.press().close ? KeyboardOutcome::Confirm : KeyboardOutcome::Handled;
    case gamepad::Button::B:
        // A character at a time; with none left B leaves the keyboard.
        if (keyboard.text().empty()) {
            return KeyboardOutcome::Leave;
        }
        keyboard.backspace();
        break;
    case gamepad::Button::X:
        if (keyboard.secret()) {
            keyboard.toggleShift();
        }
        break;
    case gamepad::Button::Y:
        static_cast<void>(keyboard.type(" "));
        break;
    case gamepad::Button::Select:
        static_cast<void>(keyboard.clear());
        break;
    case gamepad::Button::Start:
        return KeyboardOutcome::Confirm;
    default:
        break;
    }
    return KeyboardOutcome::Handled;
}

} // namespace opensu::app

#include "keyboard_pad.hpp"

namespace opensu::app {
namespace {

KeyboardOutcome editedIf(bool changed) noexcept {
    return changed ? KeyboardOutcome::Edited : KeyboardOutcome::Handled;
}

KeyboardOutcome onGrid(ui::OnScreenKeyboard& keyboard, gamepad::Button button,
                       audio::SdlSoundPlayer& sounds) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (keyboard.move(directionOf(button))) {
            sounds.play(audio::Effect::Navigation);
        }
        return KeyboardOutcome::Handled;
    case gamepad::Button::A: {
        const ui::KeyPress press = keyboard.press();
        return press.done ? KeyboardOutcome::Confirm : editedIf(press.edited);
    }
    case gamepad::Button::B:
        // A character at a time; with none left B leaves the keyboard.
        if (keyboard.text().empty()) {
            return KeyboardOutcome::Leave;
        }
        return editedIf(keyboard.backspace());
    case gamepad::Button::X:
        if (keyboard.secret()) {
            keyboard.toggleShift();
        }
        return KeyboardOutcome::Handled;
    case gamepad::Button::Y:
        return editedIf(keyboard.typeSpace());
    default:
        return KeyboardOutcome::Handled;
    }
}

/// The face a face button types on a petal.
std::optional<ui::DaisyFace> faceOf(gamepad::Button button) noexcept {
    switch (button) {
    case gamepad::Button::A:
        return ui::DaisyFace::A;
    case gamepad::Button::B:
        return ui::DaisyFace::B;
    case gamepad::Button::X:
        return ui::DaisyFace::X;
    case gamepad::Button::Y:
        return ui::DaisyFace::Y;
    default:
        return std::nullopt;
    }
}

KeyboardOutcome onWheel(ui::OnScreenKeyboard& keyboard, gamepad::Button button,
                        audio::SdlSoundPlayer& sounds) {
    if (const std::optional<ui::DaisyFace> face = faceOf(button)) {
        // A clicked character is typed by the A the pointer presses.
        if (*face == ui::DaisyFace::A && keyboard.pointed()) {
            return editedIf(keyboard.press().edited);
        }
        if (keyboard.petal()) {
            return editedIf(keyboard.typeFace(*face));
        }
    }
    switch (button) {
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (keyboard.moveCursor(button == gamepad::Button::Left ? -1 : 1)) {
            sounds.play(audio::Effect::Navigation);
        }
        return KeyboardOutcome::Handled;
    case gamepad::Button::B:
        return KeyboardOutcome::Leave;
    case gamepad::Button::X:
    case gamepad::Button::L1:
        return editedIf(keyboard.backspace());
    case gamepad::Button::Y:
    case gamepad::Button::R1:
        return editedIf(keyboard.typeSpace());
    case gamepad::Button::L2:
    case gamepad::Button::R2:
        keyboard.cycleSet(button == gamepad::Button::L2 ? -1 : 1);
        sounds.play(audio::Effect::Navigation);
        return KeyboardOutcome::Handled;
    default:
        return KeyboardOutcome::Handled;
    }
}

} // namespace

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

KeyboardOutcome actOnKeyboard(ui::OnScreenKeyboard& keyboard, gamepad::Button button,
                              audio::SdlSoundPlayer& sounds) {
    switch (button) {
    case gamepad::Button::Select:
        return editedIf(keyboard.clear());
    case gamepad::Button::Start:
        return KeyboardOutcome::Confirm;
    default:
        break;
    }
    return keyboard.style() == input::KeyboardStyle::DaisyWheel ? onWheel(keyboard, button, sounds)
                                                                : onGrid(keyboard, button, sounds);
}

} // namespace opensu::app

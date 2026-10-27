#include "keyboard_pad.hpp"

#include <optional>
#include <string>

#include "gpu2d/ui/widgets.hpp"

namespace opensu::app {
namespace {

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

void KeyboardPad::open() {
    if (keyboard_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    keyboard_.open();
    navigator_.edit(true);
}

void KeyboardPad::close() {
    if (!keyboard_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    keyboard_.close();
    navigator_.edit(false);
}

void KeyboardPad::follow() {
    if (keyboard_.isOpen() && !navigator_.editing()) {
        keyboard_.close();
    }
}

bool KeyboardPad::takesStick() const noexcept {
    return keyboard_.isOpen() && keyboard_.style() == input::KeyboardStyle::DaisyWheel;
}

void KeyboardPad::stick(ui::Direction direction, bool held) {
    // input-sound.md 3.4 Navigation: a focus move.
    if (keyboard_.stick(direction, held)) {
        sounds_.play(audio::Effect::Navigation);
    }
}

bool KeyboardPad::fieldEmpty() const {
    const auto* field =
        dynamic_cast<const gpu2d::ui::TextField*>(navigator_.find(navigator_.focused()));
    return field == nullptr || field->text().empty();
}

void KeyboardPad::type(const std::string& typed) {
    if (!typed.empty()) {
        navigator_.text(typed);
    }
}

void KeyboardPad::erase() {
    navigator_.act(gpu2d::ui::Action::Erase);
}

void KeyboardPad::done() {
    close();
    navigator_.act(gpu2d::ui::Action::Submit);
}

void KeyboardPad::apply(const ui::KeyPress& press) {
    if (press.done) {
        done();
        return;
    }
    if (press.erase) {
        erase();
    }
    type(press.typed);
}

void KeyboardPad::act(gamepad::Button button) {
    if (button == gamepad::Button::Start) {
        done();
        return;
    }
    if (button == gamepad::Button::Select) {
        // Y types on the keyboard, so Select stands for the field's Clear.
        navigator_.act(gpu2d::ui::Action::Tertiary);
        return;
    }
    if (keyboard_.style() == input::KeyboardStyle::DaisyWheel) {
        onWheel(button);
    } else {
        onGrid(button);
    }
}

void KeyboardPad::onGrid(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (keyboard_.move(directionOf(button))) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        apply(keyboard_.press());
        break;
    case gamepad::Button::B:
        // A character at a time; with none left B closes the keyboard.
        if (fieldEmpty()) {
            close();
        } else {
            erase();
        }
        break;
    case gamepad::Button::X:
        keyboard_.toggleShift();
        break;
    case gamepad::Button::Y:
        type(" ");
        break;
    default:
        break;
    }
}

void KeyboardPad::onWheel(gamepad::Button button) {
    if (const std::optional<ui::DaisyFace> face = faceOf(button)) {
        // A clicked character is typed by the A the pointer presses.
        if (*face == ui::DaisyFace::A && keyboard_.pointed()) {
            apply(keyboard_.press());
            return;
        }
        if (keyboard_.petal()) {
            type(keyboard_.typeFace(*face));
            return;
        }
    }
    switch (button) {
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        // input-sound.md 3.4 Navigation: a caret move.
        if (navigator_.act(button == gamepad::Button::Left ? gpu2d::ui::Action::Left
                                                           : gpu2d::ui::Action::Right)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::B:
        close();
        break;
    case gamepad::Button::X:
    case gamepad::Button::L1:
        erase();
        break;
    case gamepad::Button::Y:
    case gamepad::Button::R1:
        type(" ");
        break;
    case gamepad::Button::L2:
    case gamepad::Button::R2:
        keyboard_.cycleSet(button == gamepad::Button::L2 ? -1 : 1);
        sounds_.play(audio::Effect::Navigation);
        break;
    default:
        break;
    }
}

} // namespace opensu::app

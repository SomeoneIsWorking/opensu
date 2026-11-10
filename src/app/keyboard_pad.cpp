#include "keyboard_pad.hpp"

#include <optional>
#include <string>

#include "gpu2d/ui/widgets.hpp"
#include "lucent/log.h"

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
    if (keyboard_.isOpen() || systemTyping_) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    if (navigator_.touch() && systemKeyboard_) {
        systemTyping_ = true;
    } else {
        keyboard_.open(navigator_.touch());
    }
    navigator_.edit(true);
}

void KeyboardPad::openOverGame(GameTyping& game) {
    if (keyboard_.isOpen() || systemTyping_) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    game_ = &game;
    keyboard_.openOverGame(navigator_.touch());
}

void KeyboardPad::close() {
    if (!keyboard_.isOpen() && !systemTyping_) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    keyboard_.close();
    systemTyping_ = false;
    if (game_ != nullptr) {
        game_ = nullptr;
        return;
    }
    navigator_.edit(false);
}

void KeyboardPad::follow(bool inGame) {
    if (game_ != nullptr) {
        if (navigator_.active() || !inGame) {
            close();
        }
        return;
    }
    if (!navigator_.editing()) {
        keyboard_.close();
        systemTyping_ = false;
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
    if (game_ != nullptr) {
        return keyboard_.preview().value_or(std::string{}).empty();
    }
    const auto* field =
        dynamic_cast<const gpu2d::ui::TextField*>(navigator_.find(navigator_.focused()));
    return field == nullptr || field->text().empty();
}

void KeyboardPad::type(const std::string& typed) {
    if (typed.empty()) {
        return;
    }
    if (game_ == nullptr) {
        navigator_.text(typed);
        return;
    }
    if (!game_->type(typed)) {
        lucent::warn("keyboard", "the game's display has no key for \"{}\"", typed);
        return;
    }
    keyboard_.echo(typed);
}

void KeyboardPad::erase() {
    if (game_ == nullptr) {
        navigator_.act(gpu2d::ui::Action::Erase);
        return;
    }
    game_->erase();
    keyboard_.echoErase();
}

bool KeyboardPad::caret(bool left) {
    if (game_ == nullptr) {
        return navigator_.act(left ? gpu2d::ui::Action::Left : gpu2d::ui::Action::Right);
    }
    game_->caret(left);
    return true;
}

void KeyboardPad::done() {
    if (game_ != nullptr) {
        game_->enter();
        close();
        return;
    }
    close();
    navigator_.act(gpu2d::ui::Action::Submit);
}

void KeyboardPad::apply(const ui::KeyPress& press) {
    if (press.done) {
        done();
        return;
    }
    if (press.paste && game_ == nullptr) {
        navigator_.paste();
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
        if (game_ != nullptr) {
            return;
        }
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
        if (caret(button == gamepad::Button::Left)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        if (game_ == nullptr) {
            navigator_.paste();
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

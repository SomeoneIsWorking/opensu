#include "keyboard_bindings.hpp"

namespace opensu::input {

void KeyFrame::keyDown(int key, bool repeat) {
    if (repeat) {
        repeated_.insert(key);
        return;
    }
    down_.insert(key);
    pressed_.insert(key);
}

void KeyFrame::keyUp(int key) {
    down_.erase(key);
    released_.insert(key);
}

void KeyFrame::type(std::string_view text) {
    typed_.append(text);
}

void KeyFrame::nextFrame() {
    pressed_.clear();
    repeated_.clear();
    released_.clear();
    typed_.clear();
}

bool KeyFrame::down(int key) const {
    return down_.contains(key);
}

bool KeyFrame::pressed(int key) const {
    return pressed_.contains(key);
}

bool KeyFrame::released(int key) const {
    return released_.contains(key);
}

bool KeyFrame::pressedOrRepeated(int key) const {
    return pressed_.contains(key) || repeated_.contains(key);
}

std::optional<Combo> pressedCombo(const KeySource& keys) {
    const bool ctrl = keys.down(keys::LeftControl) || keys.down(keys::RightControl);
    const bool shift = keys.down(keys::LeftShift) || keys.down(keys::RightShift);
    for (const int key : bindableKeys()) {
        if (keys.pressed(key)) {
            return Combo{key, ctrl, shift};
        }
    }
    return std::nullopt;
}

TextInput readTextInput(const KeyFrame& keys) {
    TextInput input;
    input.text = keys.typed();
    input.backspace = keys.pressedOrRepeated(keys::Backspace);
    input.enter = keys.pressed(keys::Enter) || keys.pressed(keys::KeypadEnter);
    input.escape = keys.pressed(keys::Escape);
    input.up = keys.pressedOrRepeated(keys::Up);
    input.down = keys.pressedOrRepeated(keys::Down);
    input.left = keys.pressedOrRepeated(keys::Left);
    input.right = keys.pressedOrRepeated(keys::Right);
    return input;
}

} // namespace opensu::input

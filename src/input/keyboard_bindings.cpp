#include "keyboard_bindings.hpp"

#include "raylib.h"

namespace opensu::input {
namespace {

class RaylibKeys final : public KeySource {
  public:
    [[nodiscard]] bool down(int key) const override {
        return IsKeyDown(key);
    }
    [[nodiscard]] bool pressed(int key) const override {
        return IsKeyPressed(key);
    }
    [[nodiscard]] bool released(int key) const override {
        return IsKeyReleased(key);
    }
};

const RaylibKeys raylib;

} // namespace

const KeySource& raylibKeys() noexcept {
    return raylib;
}

std::optional<Combo> pressedCombo(const KeySource& keys) {
    const bool ctrl = keys.down(KEY_LEFT_CONTROL) || keys.down(KEY_RIGHT_CONTROL);
    const bool shift = keys.down(KEY_LEFT_SHIFT) || keys.down(KEY_RIGHT_SHIFT);
    for (const int key : bindableKeys()) {
        if (keys.pressed(key)) {
            return Combo{key, ctrl, shift};
        }
    }
    return std::nullopt;
}

TextInput readTextInput() {
    TextInput input;
    for (int point = GetCharPressed(); point != 0; point = GetCharPressed()) {
        int bytes = 0;
        const char* encoded = CodepointToUTF8(point, &bytes);
        input.text.append(encoded, static_cast<std::size_t>(bytes));
    }
    const auto pressed = [](int key) {
        return IsKeyPressed(key) || IsKeyPressedRepeat(key);
    };
    input.backspace = pressed(KEY_BACKSPACE);
    input.enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.up = pressed(KEY_UP);
    input.down = pressed(KEY_DOWN);
    return input;
}

} // namespace opensu::input

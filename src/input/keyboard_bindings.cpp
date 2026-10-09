#include "keyboard_bindings.hpp"

#include <array>

#include "raylib.h"

namespace opensu::input {
namespace {

using gamepad::Button;

constexpr std::array<KeyBinding, 22> bindings{{
    {KEY_UP, Button::Up},
    {KEY_W, Button::Up},
    {KEY_DOWN, Button::Down},
    {KEY_S, Button::Down},
    {KEY_LEFT, Button::Left},
    {KEY_A, Button::Left},
    {KEY_RIGHT, Button::Right},
    {KEY_D, Button::Right},
    {KEY_ENTER, Button::A},
    {KEY_SPACE, Button::A},
    {KEY_ESCAPE, Button::B},
    {KEY_F, Button::X},
    {KEY_Y, Button::Y},
    {KEY_TAB, Button::Select},
    {KEY_KB_MENU, Button::Select},
    {KEY_F10, Button::Select, false, true},
    {KEY_E, Button::Start},
    {KEY_LEFT_BRACKET, Button::L1},
    {KEY_RIGHT_BRACKET, Button::R1},
    {KEY_R, Button::R1},
    {KEY_SLASH, Button::Search},
    {KEY_F, Button::Search, true, false},
}};

struct NamedKey {
    int key;
    const char* label;
};

constexpr std::array<NamedKey, 11> namedKeys{{
    {KEY_KB_MENU, "Menu"},
    {KEY_UP, "Up"},
    {KEY_DOWN, "Down"},
    {KEY_LEFT, "Left"},
    {KEY_RIGHT, "Right"},
    {KEY_ENTER, "Enter"},
    {KEY_SPACE, "Space"},
    {KEY_ESCAPE, "Esc"},
    {KEY_TAB, "Tab"},
    {KEY_LEFT_BRACKET, "["},
    {KEY_RIGHT_BRACKET, "]"},
}};

std::string labelOf(int key) {
    for (const NamedKey& named : namedKeys) {
        if (named.key == key) {
            return named.label;
        }
    }
    // Letter keys carry their ASCII capital as the code.
    return std::string(1, static_cast<char>(key));
}

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

std::span<const KeyBinding> keyBindings() noexcept {
    return bindings;
}

const KeySource& raylibKeys() noexcept {
    return raylib;
}

std::vector<gamepad::Event> keyEvents(const KeySource& keys) {
    const bool ctrl = keys.down(KEY_LEFT_CONTROL) || keys.down(KEY_RIGHT_CONTROL);
    const bool shift = keys.down(KEY_LEFT_SHIFT) || keys.down(KEY_RIGHT_SHIFT);
    std::vector<gamepad::Event> events;
    for (const KeyBinding& binding : bindings) {
        if (binding.ctrl != ctrl || binding.shift != shift) {
            continue;
        }
        if (keys.pressed(binding.key)) {
            events.push_back(
                gamepad::Event{.button = binding.button, .pressed = true, .device = {}});
        } else if (keys.released(binding.key)) {
            events.push_back(
                gamepad::Event{.button = binding.button, .pressed = false, .device = {}});
        }
    }
    return events;
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

std::optional<std::string> keyLabelFor(Button button) {
    for (const KeyBinding& binding : bindings) {
        if (binding.button == button) {
            return labelOf(binding.key);
        }
    }
    return std::nullopt;
}

Button buttonOfGlyph(std::string_view glyph) noexcept {
    if (glyph == "A") {
        return Button::A;
    }
    if (glyph == "B") {
        return Button::B;
    }
    if (glyph == "X") {
        return Button::X;
    }
    if (glyph == "Y") {
        return Button::Y;
    }
    if (glyph == "-") {
        return Button::Select;
    }
    if (glyph == "+") {
        return Button::Start;
    }
    if (glyph == "LB") {
        return Button::L1;
    }
    if (glyph == "RB") {
        return Button::R1;
    }
    return Button::None;
}

std::optional<std::string> keyLabelForGlyph(std::string_view glyph) {
    const Button button = buttonOfGlyph(glyph);
    if (button == Button::None) {
        return std::nullopt;
    }
    return keyLabelFor(button);
}

} // namespace opensu::input

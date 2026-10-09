#include "keyboard_bindings.hpp"

#include <array>

#include "raylib.h"

namespace opensu::input {
namespace {

using gamepad::Button;

constexpr std::array<KeyBinding, 18> bindings{{
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
    {KEY_E, Button::Start},
    {KEY_LEFT_BRACKET, Button::L1},
    {KEY_RIGHT_BRACKET, Button::R1},
    {KEY_R, Button::R1},
}};

struct NamedKey {
    int key;
    const char* label;
};

constexpr std::array<NamedKey, 10> namedKeys{{
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

} // namespace

std::span<const KeyBinding> keyBindings() noexcept {
    return bindings;
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

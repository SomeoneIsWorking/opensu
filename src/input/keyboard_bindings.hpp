// keyboard_bindings — the keys that stand in for the controller's buttons. The one table: the
// shell reads its keys from it and the prompts name them from it.
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "gamepad/event.hpp"

namespace opensu::input {

/// A raylib key code and the button it presses. A binding with `ctrl` is the key with Control held;
/// one without it is the key without.
struct KeyBinding {
    int key;
    gamepad::Button button;
    bool ctrl{false};
};

/// Every binding. A button's first entry is the key its prompts show.
[[nodiscard]] std::span<const KeyBinding> keyBindings() noexcept;

/// What a physical keyboard typed into a text field in one frame.
struct TextInput {
    /// The characters typed, as UTF-8.
    std::string text;
    bool backspace{false};
    bool enter{false};
    bool escape{false};
    bool up{false};
    bool down{false};

    [[nodiscard]] bool any() const noexcept {
        return !text.empty() || backspace || enter || escape || up || down;
    }
};

/// Reads the keyboard as text input from raylib. Main loop only.
[[nodiscard]] TextInput readTextInput();

/// The key a button's prompt shows, as its cap reads (`Enter`, `Esc`, `]`), or nothing when no
/// key is bound to it.
[[nodiscard]] std::optional<std::string> keyLabelFor(gamepad::Button button);

/// The button a glyph key stands for: the glyph painter's "A", "B", "X", "Y", "-", "+", "LB", "RB".
[[nodiscard]] gamepad::Button buttonOfGlyph(std::string_view glyph) noexcept;

/// The key cap text for a glyph key, or nothing when its button has no key.
[[nodiscard]] std::optional<std::string> keyLabelForGlyph(std::string_view glyph);

} // namespace opensu::input

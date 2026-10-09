// keyboard_bindings — the keys that stand in for the controller's buttons. The one table: the
// shell reads its keys from it and the prompts name them from it.
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "gamepad/event.hpp"

namespace iideck::input {

/// A raylib key code and the button it presses.
struct KeyBinding {
    int key;
    gamepad::Button button;
};

/// Every binding. A button's first entry is the key its prompts show.
[[nodiscard]] std::span<const KeyBinding> keyBindings() noexcept;

/// The key a button's prompt shows, as its cap reads (`Enter`, `Esc`, `]`), or nothing when no
/// key is bound to it.
[[nodiscard]] std::optional<std::string> keyLabelFor(gamepad::Button button);

/// The button a glyph key stands for: the glyph painter's "A", "B", "X", "Y", "-", "+", "LB", "RB".
[[nodiscard]] gamepad::Button buttonOfGlyph(std::string_view glyph) noexcept;

/// The key cap text for a glyph key, or nothing when its button has no key.
[[nodiscard]] std::optional<std::string> keyLabelForGlyph(std::string_view glyph);

} // namespace iideck::input

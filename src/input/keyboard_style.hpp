// keyboard_style — which on-screen keyboard the player chose (Settings › Controls › Keyboard): the
// daisy wheel or the regular key grid, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::input {

enum class KeyboardStyle : std::uint8_t {
    /// Eight petals of four characters, picked with the left stick and typed with A, B, X and Y.
    DaisyWheel,
    /// A grid of keys walked with the D-pad.
    Regular,
};

inline constexpr std::array allKeyboardStyles{KeyboardStyle::DaisyWheel, KeyboardStyle::Regular};

/// The style as the settings file spells it: "daisy", "regular".
[[nodiscard]] std::string_view key(KeyboardStyle style) noexcept;

/// The style a spelling names, or nothing.
[[nodiscard]] std::optional<KeyboardStyle> keyboardStyleOf(std::string_view spelling) noexcept;

/// The style as the player reads it: "Daisy wheel", "Regular".
[[nodiscard]] std::string_view label(KeyboardStyle style) noexcept;

} // namespace opensu::input

// key_names — the keys a shortcut can use, by raylib key code: their caps' names, the Ctrl and
// Shift that modify them, and their names to the X server. Free of raylib's header, so X11 code can
// use it.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace opensu::input {

/// A key with exactly the Ctrl and Shift it needs held. `key` is a raylib key code.
struct Combo {
    int key{0};
    bool ctrl{false};
    bool shift{false};

    bool operator==(const Combo&) const = default;
};

enum class Modifier : std::uint8_t { Neither, Ctrl, Shift };

/// What the key modifies, for Ctrl and Shift (either side); `Neither` for any other key.
[[nodiscard]] Modifier modifierOf(int key) noexcept;
/// The four modifier keys, as raylib codes.
[[nodiscard]] std::span<const int> modifierKeys() noexcept;
/// Every key that can be bound, as raylib codes.
[[nodiscard]] std::span<const int> bindableKeys() noexcept;
/// A key's cap name (`Enter`, `Esc`, `[`), or nothing when it cannot be bound.
[[nodiscard]] std::optional<std::string> keyName(int key);
/// `Ctrl+Shift+Tab`: a cap's name for the combination.
[[nodiscard]] std::string describe(const Combo& combo);
/// The combination spelled `text` ("ctrl+up"), or nothing for another spelling.
[[nodiscard]] std::optional<Combo> comboNamed(std::string_view text);
/// The X keysym name of a bindable or modifier key ("Return", "a", "Shift_L"), for
/// XStringToKeysym; empty for another key.
[[nodiscard]] std::string x11Name(int key);

} // namespace opensu::input

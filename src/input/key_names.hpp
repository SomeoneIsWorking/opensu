// key_names — the keyboard's key codes, the keys a shortcut can use, their caps' names, the Ctrl
// and Shift that modify them, and their names to the X server. Pure, so the window, X11 code and
// tests share it.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace opensu::input {

/// Key codes: a letter or digit is its ASCII capital or digit, other keys sit above 255.
namespace keys {
inline constexpr int Space = 32;
inline constexpr int Comma = 44;
inline constexpr int Minus = 45;
inline constexpr int Period = 46;
inline constexpr int Slash = 47;
inline constexpr int Zero = 48;
inline constexpr int Nine = 57;
inline constexpr int Semicolon = 59;
inline constexpr int Equal = 61;
inline constexpr int A = 65;
inline constexpr int D = 68;
inline constexpr int E = 69;
inline constexpr int F = 70;
inline constexpr int I = 73;
inline constexpr int M = 77;
inline constexpr int Q = 81;
inline constexpr int R = 82;
inline constexpr int S = 83;
inline constexpr int W = 87;
inline constexpr int Y = 89;
inline constexpr int Z = 90;
inline constexpr int LeftBracket = 91;
inline constexpr int RightBracket = 93;
inline constexpr int Escape = 256;
inline constexpr int Enter = 257;
inline constexpr int Tab = 258;
inline constexpr int Backspace = 259;
inline constexpr int Right = 262;
inline constexpr int Left = 263;
inline constexpr int Down = 264;
inline constexpr int Up = 265;
inline constexpr int PageUp = 266;
inline constexpr int PageDown = 267;
inline constexpr int Home = 268;
inline constexpr int End = 269;
inline constexpr int F1 = 290;
inline constexpr int F5 = 294;
inline constexpr int F9 = 298;
inline constexpr int F10 = 299;
inline constexpr int F12 = 301;
inline constexpr int KeypadEnter = 335;
inline constexpr int LeftShift = 340;
inline constexpr int LeftControl = 341;
inline constexpr int LeftAlt = 342;
inline constexpr int RightShift = 344;
inline constexpr int RightControl = 345;
inline constexpr int RightAlt = 346;
inline constexpr int Menu = 348;
} // namespace keys

/// A key with exactly the Ctrl and Shift it needs held. `key` is a key code.
struct Combo {
    int key{0};
    bool ctrl{false};
    bool shift{false};

    bool operator==(const Combo&) const = default;
};

enum class Modifier : std::uint8_t { Neither, Ctrl, Shift };

/// What the key modifies, for Ctrl and Shift (either side); `Neither` for any other key.
[[nodiscard]] Modifier modifierOf(int key) noexcept;
/// The four modifier keys.
[[nodiscard]] std::span<const int> modifierKeys() noexcept;
/// Every key that can be bound.
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

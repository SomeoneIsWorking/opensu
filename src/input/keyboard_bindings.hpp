// keyboard_bindings — the keys that stand in for the controller's buttons. The one table: the
// shell reads its keys from it and the prompts name them from it.
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gamepad/event.hpp"

namespace opensu::input {

/// A raylib key code and the button it presses. A binding fires only with exactly its modifiers
/// held: `ctrl` and `shift` mean the key with that modifier down, their absence the key without it.
struct KeyBinding {
    int key;
    gamepad::Button button;
    bool ctrl{false};
    bool shift{false};
};

/// Every binding. A button's first entry is the key its prompts show.
[[nodiscard]] std::span<const KeyBinding> keyBindings() noexcept;

/// The keyboard's state this frame, as raylib reports it; tests give it a table instead.
class KeySource {
  public:
    virtual ~KeySource() = default;
    [[nodiscard]] virtual bool down(int key) const = 0;
    [[nodiscard]] virtual bool pressed(int key) const = 0;
    [[nodiscard]] virtual bool released(int key) const = 0;
};

/// raylib's keyboard. Main loop only.
[[nodiscard]] const KeySource& raylibKeys() noexcept;

/// The button edges the keys in `keys` made this frame, by the binding table. A key reports its
/// edges like a pad's button, so a held arrow repeats on the pad's schedule.
[[nodiscard]] std::vector<gamepad::Event> keyEvents(const KeySource& keys);

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

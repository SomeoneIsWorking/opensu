// keyboard_bindings — the keyboard as raylib reports it, and the text it types. Which key does what
// is `Shortcuts`' table.
#pragma once

#include <optional>
#include <string>

#include "key_names.hpp"

namespace opensu::input {

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

/// The combination started by a key that went down this frame, with the Ctrl and Shift held; the
/// first of `bindableKeys()` when several did.
[[nodiscard]] std::optional<Combo> pressedCombo(const KeySource& keys);

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

} // namespace opensu::input

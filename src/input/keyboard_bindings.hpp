// keyboard_bindings — the keyboard as one frame of window events left it, and the text it typed.
// Which key does what is `Shortcuts`' table.
#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "key_names.hpp"

namespace opensu::input {

/// The keyboard's state this frame; tests give it a table instead.
class KeySource {
  public:
    virtual ~KeySource() = default;
    [[nodiscard]] virtual bool down(int key) const = 0;
    [[nodiscard]] virtual bool pressed(int key) const = 0;
    [[nodiscard]] virtual bool released(int key) const = 0;
};

/// The keys and text one frame's events gave. The window feeds it; a new frame keeps the held keys
/// and forgets the edges and the text.
class KeyFrame final : public KeySource {
  public:
    /// A key went down, or repeated while held.
    void keyDown(int key, bool repeat);
    void keyUp(int key);
    /// Text the keyboard typed, as UTF-8.
    void type(std::string_view text);
    /// Starts the next frame.
    void nextFrame();

    [[nodiscard]] bool down(int key) const override;
    [[nodiscard]] bool pressed(int key) const override;
    [[nodiscard]] bool released(int key) const override;
    /// Whether the key went down or repeated this frame.
    [[nodiscard]] bool pressedOrRepeated(int key) const;
    [[nodiscard]] const std::string& typed() const noexcept {
        return typed_;
    }

  private:
    std::set<int> down_;
    std::set<int> pressed_;
    std::set<int> repeated_;
    std::set<int> released_;
    std::string typed_;
};

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
    bool left{false};
    bool right{false};

    [[nodiscard]] bool any() const noexcept {
        return !text.empty() || backspace || enter || escape || up || down || left || right;
    }
};

/// The frame's keyboard as text input.
[[nodiscard]] TextInput readTextInput(const KeyFrame& keys);

} // namespace opensu::input

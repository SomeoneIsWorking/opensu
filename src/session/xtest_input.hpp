// xtest_input — input made on an X display through XTest, as if from its own keyboard and
// pointer: keys by X keycode, pointer buttons and motion, characters typed through the display's
// own key map (with Shift where the map puts the character on its second level) and the editing
// keys. Inside Gamescope the display is its Xwayland, so the input reaches the window Gamescope
// focused and never the desktop. Keys, buttons and motion wait for `flush`; a typed character or
// an editing key is sent at once.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace opensu::session {

/// The editing keys a typist presses.
enum class EditKey : std::uint8_t { Backspace, Return, Left, Right };

class XTestInput {
  public:
    /// Opens the X display `name`; throws std::runtime_error when it cannot be opened or has no
    /// XTest.
    explicit XTestInput(const std::string& name);
    ~XTestInput();
    XTestInput(const XTestInput&) = delete;
    XTestInput& operator=(const XTestInput&) = delete;
    XTestInput(XTestInput&&) = delete;
    XTestInput& operator=(XTestInput&&) = delete;

    void key(unsigned keycode, bool down);
    void button(unsigned button, bool down);
    /// Moves the pointer to `x`, `y` across the screen, each 0 to 1.
    void move(double x, double y);
    /// Taps the key that types the printable ASCII `character`; false when the key map has none.
    bool type(char character);
    /// Taps `key`; false when the key map has no key for it.
    bool tap(EditKey key);
    void flush();

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
};

} // namespace opensu::session

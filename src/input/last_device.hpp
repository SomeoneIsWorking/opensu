// last_device — which device produced the shell's most recent real input, so prompts can name the
// controller's buttons or the keyboard's keys, and touch controls show while a pointer leads.
#pragma once

#include <cstdint>
#include <string_view>

#include "gamepad/event.hpp"

namespace opensu::input {

enum class Device : std::uint8_t {
    Pad,
    KeyboardMouse,
    /// A finger on the screen; prompts show the controller's buttons.
    Touch,
};

class LastDevice {
  public:
    /// `start` is taken as the latest input until any comes.
    explicit LastDevice(Device start = Device::Pad) noexcept
        : current_{start}, pointing_{start == Device::Touch} {
    }

    [[nodiscard]] Device current() const noexcept {
        return current_;
    }
    /// Whether the latest input was a mouse or a finger, which shows the touch controls.
    [[nodiscard]] bool pointing() const noexcept {
        return pointing_;
    }

    /// A pad's event. Only a button change counts; a pad connecting or leaving is not input, and a
    /// stick below the pad translator's threshold never becomes a button event.
    void notePad(const gamepad::Event& event) noexcept;

    /// A key press or release.
    void noteKey() noexcept {
        current_ = Device::KeyboardMouse;
        pointing_ = false;
    }

    /// The pointer this frame: how far it moved, whether a button went down or came up, and
    /// whether a finger drove it. A still pointer is not input.
    void notePointer(float dx, float dy, bool button, bool touch) noexcept;

  private:
    Device current_{Device::Pad};
    bool pointing_{false};
};

/// The device as the control channel's `/state` spells it: "pad", "keyboard" or "touch".
[[nodiscard]] std::string_view name(Device device) noexcept;

} // namespace opensu::input

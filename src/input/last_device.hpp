// last_device — which device produced the shell's most recent real input, so prompts can name the
// controller's buttons or the keyboard's keys.
#pragma once

#include <cstdint>

#include "gamepad/event.hpp"

namespace iideck::input {

enum class Device : std::uint8_t {
    Pad,
    KeyboardMouse,
};

class LastDevice {
  public:
    [[nodiscard]] Device current() const noexcept {
        return current_;
    }

    /// A pad's event. Only a button change counts; a pad connecting or leaving is not input, and a
    /// stick below the pad translator's threshold never becomes a button event.
    void notePad(const gamepad::Event& event) noexcept;

    /// A key press or release.
    void noteKey() noexcept {
        current_ = Device::KeyboardMouse;
    }

    /// The pointer this frame: how far it moved and whether a button went down. A still pointer
    /// is not input.
    void notePointer(float dx, float dy, bool buttonPressed) noexcept;

  private:
    Device current_{Device::Pad};
};

} // namespace iideck::input

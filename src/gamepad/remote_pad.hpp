// remote_pad — a pad on another openSU, played here: a virtual pad of opensu's own, set to each
// state the other side reports as a whole (buttons held, sticks, triggers). Only what changed is
// written. Guide is not part of a state; it belongs to the openSU it is pressed on.
#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "pad_translator.hpp"
#include "virtual_pad.hpp"

namespace opensu::gamepad {

/// A pad as it stands: the controls held (bit `Button` value), the sticks on `virtualStick`'s
/// scale and the triggers on `virtualTrigger`'s.
struct PadSnapshot {
    std::uint32_t buttons{0};
    Sticks sticks;
    std::int32_t leftTrigger{0};
    std::int32_t rightTrigger{0};

    [[nodiscard]] bool holds(Button button) const noexcept {
        return (buttons >> static_cast<unsigned>(button) & 1U) != 0;
    }
};

class RemotePad {
  public:
    /// Creates the virtual pad; throws `std::system_error` when uinput is unavailable.
    RemotePad() = default;

    /// Takes the virtual pad to `pad`.
    void follow(const PadSnapshot& pad);
    /// Takes it back to rest.
    void rest();

  private:
    VirtualPad pad_;
    /// The virtual pad's state as last written, by (type, code).
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::int32_t> state_;
};

} // namespace opensu::gamepad

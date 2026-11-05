// remote_pad — a pad on another openSU, played here: a virtual pad of opensu's own, set to each
// state the other side reports as a whole (buttons held, sticks, triggers' travel) through that
// side's mapping for the game (`ForwardMap`), applied as a pad held here applies its own
// (`PadTranslator`). Only what changed is written. Guide is not part of a state; it belongs to
// the openSU it is pressed on.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
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
    /// Plays `seat`, a pad lent by this openSU's own pads, or else creates a virtual pad; throws
    /// `std::system_error` when uinput is unavailable.
    explicit RemotePad(std::shared_ptr<VirtualPad> seat);

    /// Plays the pad with `map` from now on, a modifier alone a tap for `holdDelay`.
    void setMapping(ForwardMap map, std::chrono::milliseconds holdDelay);
    /// Takes the virtual pad to `pad`.
    void follow(const PadSnapshot& pad);
    /// Takes it back to rest.
    void rest();
    /// The virtual pad's evdev node.
    [[nodiscard]] const std::filesystem::path& node() const noexcept {
        return pad_->node();
    }

  private:
    std::shared_ptr<VirtualPad> pad_;
    /// The other side's pad as a pad that reads like the virtual one, through the mapping.
    PadTranslator translator_;
    /// The state given to the translator last, by (type, code).
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::int32_t> state_;
};

} // namespace opensu::gamepad

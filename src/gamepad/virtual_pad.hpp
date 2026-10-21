// virtual_pad — a uinput gamepad with the Xbox 360 pad's identity and layout, which a game reads
// in place of the grabbed physical pad.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "pad_translator.hpp"

namespace opensu::gamepad {

class VirtualPad {
  public:
    /// What marks opensu's own virtual pads, so they are never grabbed or read as a controller.
    static constexpr std::string_view phys{"opensu/virtual-pad"};
    static constexpr std::string_view name{"opensu virtual pad"};
    /// Xbox 360 wired: the identity every game and SDL's mapping database knows.
    static constexpr std::uint16_t vendor{0x045e};
    static constexpr std::uint16_t product{0x028e};

    /// Creates the device and returns once its node is open to this user, as it is to a game;
    /// throws std::system_error when uinput is unavailable.
    VirtualPad();
    VirtualPad(const VirtualPad&) = delete;
    VirtualPad& operator=(const VirtualPad&) = delete;
    ~VirtualPad();

    /// Writes the events as the pad's input.
    void write(const std::vector<PadEvent>& events);

    /// Its evdev node, `/dev/input/event21`; empty when uinput did not say.
    [[nodiscard]] const std::filesystem::path& node() const noexcept {
        return node_;
    }

  private:
    /// Finds the new node and waits for udev to grant access to it; a game that enumerates
    /// before then cannot open it.
    void awaitAccess();

    int fd_{-1};
    std::filesystem::path node_;
};

} // namespace opensu::gamepad

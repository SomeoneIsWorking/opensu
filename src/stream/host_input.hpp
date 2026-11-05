// host_input — what one guest's player does, done on the host: their pad as a virtual pad of
// opensu's own through their mapping for the game (`gamepad::RemotePad`), and for the guest the
// game was started for their keys and pointer as XTest input on the X display of the Gamescope the
// game runs in, so they reach the game alone and never the desktop. Each state arrives whole; only
// what changed is done. Called from the stream's thread only.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "stream_wire.hpp"

namespace opensu::gamepad {
class VirtualPad;
}

namespace opensu::stream {

class HostInput {
  public:
    /// Opens a virtual pad, or plays `seat`, one lent by the pads here, and the X display
    /// `x11Display` when there is one for the keys and pointer (without, they are dropped); throws
    /// `std::runtime_error` (or `std::system_error` for uinput) saying why it cannot.
    HostInput(const std::optional<std::string>& x11Display,
              std::shared_ptr<gamepad::VirtualPad> seat);
    ~HostInput();
    HostInput(const HostInput&) = delete;
    HostInput& operator=(const HostInput&) = delete;
    HostInput(HostInput&&) = delete;
    HostInput& operator=(HostInput&&) = delete;

    void pad(const wire::PadState& state);
    void keys(const wire::KeyState& state);
    void mouse(const wire::MouseState& state);
    void mapping(const wire::PadMapping& mapping);
    /// Lets go of everything held, as the stream that played it ended.
    void rest();
    /// The evdev node of the guest's pad here.
    [[nodiscard]] std::string padNode() const;

  private:
    /// The virtual pad and the X display, and what each was last given.
    class Devices;
    std::unique_ptr<Devices> devices_;
};

} // namespace opensu::stream

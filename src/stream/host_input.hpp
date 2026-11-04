// host_input — what the guest's player does, done on the host: their pad as a virtual pad of
// opensu's own (`gamepad::RemotePad`), their keys and pointer as XTest input on the X display of
// the Gamescope the game runs in, so they reach the game alone and never the desktop. Each state
// arrives whole; only what changed is done. Called from the stream's thread only.
#pragma once

#include <memory>
#include <string>

#include "stream_wire.hpp"

namespace opensu::stream {

class HostInput {
  public:
    /// Opens the virtual pad and X display `x11Display`; throws `std::runtime_error` (or
    /// `std::system_error` for uinput) saying why it cannot.
    explicit HostInput(const std::string& x11Display);
    ~HostInput();
    HostInput(const HostInput&) = delete;
    HostInput& operator=(const HostInput&) = delete;
    HostInput(HostInput&&) = delete;
    HostInput& operator=(HostInput&&) = delete;

    void pad(const wire::PadState& state);
    void keys(const wire::KeyState& state);
    void mouse(const wire::MouseState& state);

  private:
    /// The virtual pad and the X display, and what each was last given.
    class Devices;
    std::unique_ptr<Devices> devices_;
};

} // namespace opensu::stream

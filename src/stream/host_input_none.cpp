#include <stdexcept>

#include "host_input.hpp"

namespace opensu::stream {

class HostInput::Devices {};

HostInput::HostInput(const std::string& /*x11Display*/) {
    throw std::runtime_error{"this system cannot play a remote player's input into a game"};
}

HostInput::~HostInput() = default;

void HostInput::pad(const wire::PadState& /*state*/) {
}

void HostInput::keys(const wire::KeyState& /*state*/) {
}

void HostInput::mouse(const wire::MouseState& /*state*/) {
}

} // namespace opensu::stream

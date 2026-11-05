#include <stdexcept>

#include "host_input.hpp"

namespace opensu::stream {

class HostInput::Devices {};

HostInput::HostInput(const std::optional<std::string>& /*x11Display*/,
                     std::shared_ptr<gamepad::VirtualPad> /*seat*/) {
    throw std::runtime_error{"this system cannot play a remote player's input into a game"};
}

HostInput::~HostInput() = default;

void HostInput::pad(const wire::PadState& /*state*/) {
}

void HostInput::keys(const wire::KeyState& /*state*/) {
}

void HostInput::mouse(const wire::MouseState& /*state*/) {
}

void HostInput::rest() {
}

void HostInput::mapping(const wire::PadMapping& /*mapping*/) {
}

std::string HostInput::padNode() const {
    return {};
}

} // namespace opensu::stream

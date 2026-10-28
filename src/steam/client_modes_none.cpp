#include "client_modes.hpp"

namespace opensu::steam {

/// No Steam client runs here (Android).
std::span<const Client::Mode> clientModes() noexcept {
    return {};
}

} // namespace opensu::steam

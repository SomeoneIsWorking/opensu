// client_modes — the modes this build's Steam client can run in, picked in CMakeLists.txt: Normal,
// Managed and Isolated on Linux, where openSU owns a Steam in a systemd scope or a bwrap sandbox
// (`client_modes_scoped.cpp`); none on Android (`client_modes_none.cpp`); Normal alone elsewhere,
// the player's own client (`client_modes_normal.cpp`).
#pragma once

#include <span>

#include "client.hpp"

namespace opensu::steam {

/// In the order Settings lists them.
[[nodiscard]] std::span<const Client::Mode> clientModes() noexcept;

} // namespace opensu::steam

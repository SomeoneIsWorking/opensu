// emulator_running — whether an emulator whose saves are about to be replaced is running, found by
// its process hint (`ProcessTree`), and the refusal the player is shown; the one running check
// Copy saves and LAN save sharing share.
#pragma once

#include <string>

#include "library/game_saves.hpp"

namespace opensu::launch {

/// "Close Ryujinx first: it is running"; empty when `saves`' emulator is not running or has no
/// process hint, as the built-in runner, which the shell's own launch knows.
[[nodiscard]] std::string runningRefusal(const library::saves::EmulatorSaves& saves);

} // namespace opensu::launch

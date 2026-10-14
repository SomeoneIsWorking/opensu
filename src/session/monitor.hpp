// session — reads the monitor opensu is about to cover, or the one its window is on.
#pragma once

#include <cstdint>
#include <optional>

#include "gamescope.hpp"

namespace opensu::session {

/// The primary monitor's size and refresh rate, read through SDL's X11 video. Nothing when there is
/// no display.
[[nodiscard]] std::optional<Output> readMonitor();

/// The size and refresh rate of SDL display `display`, with SDL's video already up. Nothing when
/// SDL cannot read its mode.
[[nodiscard]] std::optional<Output> displayOutput(std::uint32_t display);

} // namespace opensu::session

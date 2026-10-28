// session — reads the monitor opensu is about to cover, or the one its window is on.
#pragma once

#include <optional>

#include "display/output.hpp"

namespace opensu::session {

/// The primary monitor's size and refresh rate, read through SDL's X11 video. Nothing when there is
/// no display.
[[nodiscard]] std::optional<display::Output> readMonitor();

} // namespace opensu::session

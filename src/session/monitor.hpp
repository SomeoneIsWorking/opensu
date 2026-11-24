// session — reads the monitor opensu is about to cover, or the one its window is on.
#pragma once

#include <optional>

#include "display/output.hpp"

namespace opensu::session {

/// The primary monitor's size in pixels, its refresh rate and the desktop's scale, read through the
/// desktop's Wayland where it has one (Xwayland on a scaled desktop sees the scaled size), else
/// X11. Nothing when there is no display.
[[nodiscard]] std::optional<display::Output> readMonitor();

} // namespace opensu::session

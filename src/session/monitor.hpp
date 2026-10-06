// session — reads the monitor iideck is about to cover.
#pragma once

#include <optional>

#include "gamescope.hpp"

namespace iideck::session {

/// The current monitor's size and refresh rate, read through a hidden raylib
/// window. Nothing when no window can be opened, which is when there is no display.
[[nodiscard]] std::optional<Output> readMonitor();

} // namespace iideck::session

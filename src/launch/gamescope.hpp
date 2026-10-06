// launch — wraps a command in a nested Gamescope window.
#pragma once

#include "library/game.hpp"

namespace iideck::launch {

/// The display a nested Gamescope has to match; without it Gamescope opens 1280x720.
struct Output {
    int width{0};
    int height{0};
    /// Zero or negative leaves the refresh rate to Gamescope.
    int refreshHz{0};
};

/// `gamescope -W w -H h -w w -h h [-r hz] -f -- program args...`
[[nodiscard]] library::LaunchSpec wrapInGamescope(const Output& output,
                                                  const library::LaunchSpec& inner);

} // namespace iideck::launch

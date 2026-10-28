// session — the Gamescope command that wraps opensu itself.
#pragma once

#include <string>
#include <vector>

#include "display/output.hpp"

namespace opensu::session {

/// The arguments of `gamescope -W w -H h -w w -h h [-r hz] -f --close-focused-window -- program
/// args...` for a nested output, and `gamescope --backend drm -- program args...` for the native
/// one.
[[nodiscard]] std::vector<std::string> gamescopeArgs(const display::Output& output,
                                                     const std::string& program,
                                                     const std::vector<std::string>& args);

} // namespace opensu::session

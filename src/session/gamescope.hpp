// session — the Gamescope command that wraps iideck itself.
#pragma once

#include <string>
#include <vector>

namespace iideck::session {

/// The display a nested Gamescope has to match; without it Gamescope opens 1280x720.
struct Output {
    int width{0};
    int height{0};
    /// Zero or negative leaves the refresh rate to Gamescope.
    int refreshHz{0};
};

/// The arguments of `gamescope -W w -H h -w w -h h [-r hz] -f -- program args...`.
[[nodiscard]] std::vector<std::string> gamescopeArgs(const Output& output,
                                                     const std::string& program,
                                                     const std::vector<std::string>& args);

} // namespace iideck::session

// session — the Gamescope command that wraps opensu itself.
#pragma once

#include <string>
#include <vector>

namespace opensu::session {

/// The display Gamescope has to cover. A nested Gamescope must match its monitor, or it opens
/// 1280x720. A default (zero-sized) output is the native one: a top-level Gamescope on the DRM
/// backend, which takes the connector's own mode.
struct Output {
    int width{0};
    int height{0};
    /// Zero or negative leaves the refresh rate to Gamescope.
    int refreshHz{0};

    [[nodiscard]] bool native() const noexcept {
        return width <= 0 || height <= 0;
    }
};

/// The arguments of `gamescope -W w -H h -w w -h h [-r hz] -f --close-focused-window -- program
/// args...` for a nested output, and `gamescope --backend drm -- program args...` for the native
/// one.
[[nodiscard]] std::vector<std::string> gamescopeArgs(const Output& output,
                                                     const std::string& program,
                                                     const std::vector<std::string>& args);

} // namespace opensu::session

// session — the Gamescope command that wraps opensu itself.
#pragma once

#include <string>
#include <vector>

#include "display/output.hpp"

namespace opensu::session {

/// The arguments of `gamescope -W w -H h -w w -h h [-r hz] -f --close-focused-window
/// --expose-wayland -- program args...` for a nested output, and `gamescope --backend drm
/// --expose-wayland -- program args...` for the native one. With `--expose-wayland` Gamescope
/// names its own socket in its children's WAYLAND_DISPLAY (and XDG_SESSION_TYPE=wayland), where
/// otherwise it leaves WAYLAND_DISPLAY empty.
[[nodiscard]] std::vector<std::string> gamescopeArgs(const display::Output& output,
                                                     const std::string& program,
                                                     const std::vector<std::string>& args);

} // namespace opensu::session

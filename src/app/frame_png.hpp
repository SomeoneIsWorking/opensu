// frame_png — the drawn shell as PNG bytes, for the control channel's frame and `--render`.
#pragma once

#include <string>

#include "ui/shell.hpp"

namespace opensu::app {

/// Draws `shell` into an offscreen target the size of the window and encodes it as PNG. Main loop
/// only, with a live GL context. False, with the reason logged, when it cannot.
[[nodiscard]] bool renderShellPng(ui::Shell& shell, std::string& png);

} // namespace opensu::app

// frame_png — the drawn shell as PNG bytes, for the control channel's frame and `--render`.
#pragma once

#include <string>

#include "shell_window.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

/// Draws `shell` into an offscreen target the size of `window` and encodes it as PNG, in memory.
/// Icons the draw asks for are waited for and the frame drawn again with them. Main loop only.
/// False, with the reason logged, when it cannot encode; a GPU failure throws
/// gpu2d::GpuError.
[[nodiscard]] bool renderShellPng(ShellWindow& window, ui::Shell& shell, std::string& png);

} // namespace opensu::app

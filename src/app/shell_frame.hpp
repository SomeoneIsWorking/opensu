// shell_frame — the drawn shell's pixels, for the control channel's frame and `--render`.
#pragma once

#include "gpu2d/image/bitmap.hpp"
#include "shell_window.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

/// Draws `shell` into an offscreen target the size of `window` and reads it back. Icons the draw
/// asks for are waited for and the frame drawn again with them. Main loop only; a GPU failure
/// throws gpu2d::GpuError.
[[nodiscard]] gpu2d::image::Bitmap renderShell(ShellWindow& window, ui::Shell& shell);

} // namespace opensu::app

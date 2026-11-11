#include "shell_frame.hpp"

namespace opensu::app {

gpu2d::image::Bitmap renderShell(ShellWindow& window, ui::Shell& shell) {
    const auto draw = [&shell](gpu2d::Canvas& canvas, const gpu2d::Surface& target) {
        shell.draw(canvas, target);
    };
    gpu2d::image::Bitmap frame = window.render(draw);
    // A hidden window draws only these frames, so icons are first asked for here.
    if (shell.settleIcons()) {
        frame = window.render(draw);
    }
    return frame;
}

} // namespace opensu::app

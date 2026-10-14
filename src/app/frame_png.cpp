#include "frame_png.hpp"

#include "gpu2d/image/png_encoder.hpp"

#include "lucent/log.h"

namespace opensu::app {

bool renderShellPng(ShellWindow& window, ui::Shell& shell, std::string& png) {
    const gpu2d::image::Bitmap frame =
        window.render([&shell](gpu2d::Canvas& canvas, const gpu2d::Surface& target) {
            shell.draw(canvas, target);
        });
    std::string error;
    png = gpu2d::image::encodePng(frame.rgba, static_cast<unsigned>(frame.width),
                                  static_cast<unsigned>(frame.height), error);
    if (png.empty()) {
        lucent::error("render", "could not encode the frame: {}", error);
        return false;
    }
    return true;
}

} // namespace opensu::app

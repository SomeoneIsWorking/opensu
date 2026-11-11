#include "scale.hpp"

#include "lucent/log.h"
#include "panel.hpp"
#include "recommended_scale.hpp"

namespace opensu::display {

float DisplayScale::of(SDL_Window* window, Size pixels) {
    // The window system's scale follows the window from screen to screen; a recommended one only
    // changes with the screen's size.
    if (!source_.stated && source_.unscaled && decided_ > 0.0F &&
        pixels.width == decidedFor_.width && pixels.height == decidedFor_.height) {
        return decided_;
    }
    const float scale = decide(window, pixels);
    if (scale != decided_) {
        decided_ = scale;
        lucent::info("display", "{} pixels per dp on {}x{}: {}", scale, pixels.width, pixels.height,
                     source_.stated     ? "stated"
                     : source_.unscaled ? "recommended"
                                        : "the window system's");
    }
    decidedFor_ = pixels;
    return decided_;
}

float DisplayScale::decide(SDL_Window* window, Size pixels) const {
    if (source_.stated) {
        return *source_.stated;
    }
    if (source_.unscaled) {
        const Panel panel = source_.connector ? connectedPanel() : Panel{};
        lucent::info("display", "screen of {}x{}, glass {}x{} mm{}", pixels.width, pixels.height,
                     panel.widthMm, panel.heightMm, panel.builtIn ? ", built in" : "");
        return recommendedScale(pixels, panel);
    }
    const float scale = SDL_GetWindowDisplayScale(window);
    return scale > 0.0F ? scale : 1.0F;
}

} // namespace opensu::display

#include "monitor.hpp"

#include <cmath>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "lucent/log.h"

namespace opensu::session {

std::optional<display::Output> readMonitor() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        lucent::error("session", "cannot start SDL video: {}", SDL_GetError());
        return std::nullopt;
    }
    std::optional<display::Output> output = display::displayOutput(SDL_GetPrimaryDisplay());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (output) {
        // Gamescope's -W and -H are pixels.
        output->width =
            static_cast<int>(std::lround(static_cast<float>(output->width) * output->density));
        output->height =
            static_cast<int>(std::lround(static_cast<float>(output->height) * output->density));
        // Wayland gives a scaled desktop's scale as its density, the content scale on top of it.
        output->scale *= output->density;
        output->density = 1.0F;
    }
    return output;
}

} // namespace opensu::session

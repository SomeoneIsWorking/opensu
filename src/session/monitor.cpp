#include "monitor.hpp"

#include <cmath>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "lucent/log.h"

namespace opensu::session {

std::optional<Output> displayOutput(std::uint32_t display) {
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    if (mode == nullptr || mode->w <= 0 || mode->h <= 0) {
        lucent::warn("session", "no display mode for display {}: {}", display, SDL_GetError());
        return std::nullopt;
    }
    return Output{mode->w, mode->h, static_cast<int>(std::lround(mode->refresh_rate))};
}

std::optional<Output> readMonitor() {
    // The shell's window is X11 (Gamescope's overlay is an X11 property), so the monitor is read
    // as X11 sees it.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        lucent::error("session", "cannot start SDL video: {}", SDL_GetError());
        return std::nullopt;
    }
    const std::optional<Output> output = displayOutput(SDL_GetPrimaryDisplay());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return output;
}

} // namespace opensu::session

#include "output.hpp"

#include <cmath>

#include <SDL3/SDL_video.h>

#include "lucent/log.h"

namespace opensu::display {

std::optional<Output> displayOutput(std::uint32_t display) {
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    if (mode == nullptr || mode->w <= 0 || mode->h <= 0) {
        lucent::warn("session", "no display mode for display {}: {}", display, SDL_GetError());
        return std::nullopt;
    }
    return Output{mode->w, mode->h, static_cast<int>(std::lround(mode->refresh_rate))};
}

} // namespace opensu::display

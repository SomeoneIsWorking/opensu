#include "content_insets.hpp"

namespace opensu::app {

gpu2d::ui::Insets contentInsets(SDL_Window* window) {
    SDL_Rect safe{};
    int width = 0;
    int height = 0;
    const float density = SDL_GetWindowPixelDensity(window);
    if (density <= 0.0f || !SDL_GetWindowSafeArea(window, &safe) ||
        !SDL_GetWindowSize(window, &width, &height)) {
        return {};
    }
    return gpu2d::ui::Insets{.left = static_cast<float>(safe.x) * density,
                             .top = static_cast<float>(safe.y) * density,
                             .right = static_cast<float>(width - safe.x - safe.w) * density,
                             .bottom = static_cast<float>(height - safe.y - safe.h) * density};
}

} // namespace opensu::app

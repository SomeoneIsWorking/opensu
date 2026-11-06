#include "touch/sdl_touch.hpp"

namespace opensu::touch {

Finger fingerOf(SDL_Window* window, const SDL_TouchFingerEvent& event) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    Finger::Phase phase = Finger::Phase::Move;
    if (event.type == SDL_EVENT_FINGER_DOWN) {
        phase = Finger::Phase::Down;
    } else if (event.type != SDL_EVENT_FINGER_MOTION) {
        phase = Finger::Phase::Up;
    }
    return Finger{static_cast<std::uint64_t>(event.fingerID), event.x * static_cast<float>(width),
                  event.y * static_cast<float>(height), phase};
}

SDL_Event eventOf(SDL_Window* window, const Finger& finger) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    SDL_Event event{};
    switch (finger.phase) {
    case Finger::Phase::Down:
        event.type = SDL_EVENT_FINGER_DOWN;
        break;
    case Finger::Phase::Move:
        event.type = SDL_EVENT_FINGER_MOTION;
        break;
    case Finger::Phase::Up:
        event.type = SDL_EVENT_FINGER_UP;
        break;
    }
    event.tfinger.windowID = SDL_GetWindowID(window);
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = static_cast<SDL_FingerID>(finger.id);
    event.tfinger.x = finger.x / static_cast<float>(width);
    event.tfinger.y = finger.y / static_cast<float>(height);
    event.tfinger.pressure = 1.0F;
    return event;
}

WindowSurface surfaceOf(SDL_Window* window, unsigned width, unsigned height) {
    WindowSurface surface{.width = width,
                          .height = height,
                          .safeX = 0.0F,
                          .safeY = 0.0F,
                          .safeWidth = static_cast<float>(width),
                          .safeHeight = static_cast<float>(height),
                          .dp = 1.0F};
    const float density = SDL_GetWindowPixelDensity(window);
    if (const float scale = SDL_GetWindowDisplayScale(window); scale > 0.0F) {
        surface.dp = scale;
    }
    SDL_Rect safe{};
    if (density > 0.0F && SDL_GetWindowSafeArea(window, &safe)) {
        surface.safeX = static_cast<float>(safe.x) * density;
        surface.safeY = static_cast<float>(safe.y) * density;
        surface.safeWidth = static_cast<float>(safe.w) * density;
        surface.safeHeight = static_cast<float>(safe.h) * density;
    }
    return surface;
}

} // namespace opensu::touch

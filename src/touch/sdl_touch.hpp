// sdl_touch — an SDL window's fingers and surface as the on-screen pad takes them.
#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include "touch/touch_surface.hpp"

namespace opensu::touch {

/// `event` (a finger going down, moving, up or cancelled) in `window`'s pixels.
[[nodiscard]] Finger fingerOf(SDL_Window* window, const SDL_TouchFingerEvent& event);
/// `window`, whose drawing target is `width` by `height` pixels: its safe area and pixels per dp.
[[nodiscard]] WindowSurface surfaceOf(SDL_Window* window, unsigned width, unsigned height);

} // namespace opensu::touch

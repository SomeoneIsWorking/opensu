// sdl_touch — an SDL window's fingers and surface as the on-screen pad takes them.
#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include "touch/touch_surface.hpp"

namespace opensu::touch {

/// `event` (a finger going down, moving, up or cancelled) in `window`'s pixels.
[[nodiscard]] Finger fingerOf(SDL_Window* window, const SDL_TouchFingerEvent& event);
/// The same in a surface of `width` by `height` pixels that is no SDL window.
[[nodiscard]] Finger fingerOf(const SDL_TouchFingerEvent& event, unsigned width, unsigned height);
/// The event `finger`, in `window`'s pixels, comes as: a tool's way into the window's queue.
[[nodiscard]] SDL_Event eventOf(SDL_Window* window, const Finger& finger);
/// `window`, whose drawing target is `width` by `height` pixels: its safe area and pixels per dp.
[[nodiscard]] WindowSurface surfaceOf(SDL_Window* window, unsigned width, unsigned height);

} // namespace opensu::touch

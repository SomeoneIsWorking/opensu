// content_insets — how far in from each window edge the interface stands so it is seen whole.
#pragma once

#include <SDL3/SDL_video.h>

#include "gpu2d/ui/widget.hpp"

namespace opensu::app {

/// Pixels in from each edge of `window`: on Android the display cutout and the rounded corners
/// (the Activity's `contentInsets`, not SDL's safe area, which adds the hidden bars' gesture
/// strips); elsewhere SDL's safe area.
[[nodiscard]] gpu2d::ui::Insets contentInsets(SDL_Window* window);

} // namespace opensu::app

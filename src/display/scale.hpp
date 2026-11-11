// scale — the pixels per dp of the display a window is on: the one scale decision, the shell's and
// the runner's. The window system's scale where there is one (a desktop, Android's density), what
// was stated from outside, else the recommended scale for the screen (`recommended_scale`).
#pragma once

#include <optional>

#include <SDL3/SDL_video.h>

#include "letterbox.hpp"

namespace opensu::display {

/// The environment variable a scale is stated in: by a tool for a hidden run, by the player, and
/// by openSU for what runs in its nested Gamescope, which has the desktop's.
inline constexpr const char* scaleVariable = "OPENSU_DISPLAY_SCALE";

struct ScaleSource {
    /// Stated from outside (`Config::displayScale`); it wins.
    std::optional<float> stated;
    /// No window system scales this window: it is hidden, or inside Gamescope.
    bool unscaled{false};
    /// The screen is a DRM connector's (inside Gamescope, on a screen): its EDID gives the glass.
    bool connector{false};
};

class DisplayScale {
  public:
    explicit DisplayScale(ScaleSource source) noexcept : source_{source} {
    }

    /// Pixels per dp for `window`, which is `pixels` large; logged when it changes.
    [[nodiscard]] float of(SDL_Window* window, Size pixels);

  private:
    [[nodiscard]] float decide(SDL_Window* window, Size pixels) const;

    ScaleSource source_;
    Size decidedFor_;
    float decided_{0.0F};
};

} // namespace opensu::display

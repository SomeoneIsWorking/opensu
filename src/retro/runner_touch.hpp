// runner_touch — the runner's touch controls: the layer laid out for the window, the frame's
// fingers taken while it shows, and what they hold put on the joypad as the pad `touch`. A tap
// shorter than a frame is held for one, so the game sees it.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "joypad.hpp"
#include "touch/touch_layer.hpp"

namespace opensu::retro {

class RunnerTouch {
  public:
    /// The art is read from `art`; what is held goes to `joypad`.
    RunnerTouch(std::filesystem::path art, Joypad& joypad);

    /// A frame's window: lays the controls out when `surface` changed, then takes `fingers` while
    /// `shown`, or lets go. Reports whether the pause button went down.
    bool follow(const touch::WindowSurface& surface, bool shown,
                std::span<const touch::Finger> fingers);

    /// The controls as an overlay for the target.
    [[nodiscard]] touch::OverlayImage image() {
        return layer_.image();
    }

  private:
    touch::TouchLayer layer_;
    Joypad& joypad_;
    touch::WindowSurface laidOut_{};
    /// Fingers lifted in the frame they went down, let go the next.
    std::vector<touch::Finger> liftNext_;
};

} // namespace opensu::retro

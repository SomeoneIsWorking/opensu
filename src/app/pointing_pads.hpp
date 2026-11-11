// pointing_pads — pads that point at the screen (a Wii Remote's IR camera, `gamepad::WiiPointer`)
// as the shell's pointer. Where one points moves the pointer, and its pointer button (the remote's
// A while it points, as on the Wii's own menu) presses there; let go away from the screen, the
// press ends where it last pointed.
#pragma once

#include <functional>
#include <map>
#include <string>

#include "gamepad/event.hpp"

namespace opensu::app {

class PointingPads {
  public:
    struct Hooks {
        /// Moves the pointer to `x`, `y` across and down the window, from 0 to 1.
        std::function<void(float x, float y)> move;
        /// The pointer's primary button going down or up there.
        std::function<void(float x, float y, bool down)> press;
        /// The pad no longer points at the screen.
        std::function<void()> leave;
    };

    explicit PointingPads(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// Takes `event` when it is a pad's pointer or pointer button; false for anything else,
    /// which the shell handles as before.
    [[nodiscard]] bool take(const gamepad::Event& event);

  private:
    struct Spot {
        float x{};
        float y{};
    };

    Hooks hooks_;
    /// Where each pad last pointed on the screen, by `Event::source`; kept once it points away, for
    /// the release of a press made there.
    std::map<std::string, Spot> spots_;
};

} // namespace opensu::app

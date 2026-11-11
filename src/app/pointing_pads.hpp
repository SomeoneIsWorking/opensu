// pointing_pads — pads that point at the screen (a Wii Remote's IR camera, `gamepad::WiiPointer`)
// as the shell's pointer. Where one points moves the pointer, and its A while it points is the
// pointer's press there, as on the Wii's own menu; pointed away, its A is the pad's A again.
#pragma once

#include <functional>
#include <map>
#include <set>
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
    };

    explicit PointingPads(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// Takes `event` when it is a pad's pointer, or the A of a pad that points; false for
    /// anything else, which the shell handles as before.
    [[nodiscard]] bool take(const gamepad::Event& event);

  private:
    struct Aim {
        float x{};
        float y{};
        bool onScreen{false};
    };

    Hooks hooks_;
    /// Where each pad last pointed at the screen, and whether it still does, by `Event::source`.
    std::map<std::string, Aim> aims_;
    /// The pads whose A holds the pointer's button down, released by that A wherever it points.
    std::set<std::string> pressing_;
};

} // namespace opensu::app

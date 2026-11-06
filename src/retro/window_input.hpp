// window_input — what the runner's window reports each frame: closing, keys and fingers on a
// touchscreen, and the surface the overlay is drawn for.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "touch/touch_surface.hpp"

namespace opensu::retro {

/// What the window reported since the last poll.
struct WindowInput {
    struct Key {
        /// An SDL_Scancode.
        int scancode{0};
        bool down{false};
    };

    bool quit{false};
    std::vector<Key> keys;
    std::vector<touch::Finger> fingers;
    /// The window came back (true) or was minimized (false), as Android does in the background.
    std::optional<bool> shown;
};

} // namespace opensu::retro

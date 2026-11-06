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
    /// Its size changed, so the swapchain is made again.
    bool resized{false};
    /// What it drew into is gone, a new surface or none yet in its place.
    bool surfaceLost{false};
    std::vector<Key> keys;
    std::vector<touch::Finger> fingers;
    /// The window took (true) or lost (false) the input, as Android does under another Activity.
    std::optional<bool> focused;
};

} // namespace opensu::retro

// window_input — what the runner's window reports each frame: closing, keys and fingers on a
// touchscreen, and the surface the overlay is drawn for.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace opensu::retro {

/// A finger on the touchscreen, in the target's pixels.
struct Finger {
    enum class Phase : std::uint8_t {
        Down,
        Move,
        Up,
    };

    std::uint64_t id{0};
    float x{0.0F};
    float y{0.0F};
    Phase phase{Phase::Down};
};

/// What the window reported since the last poll.
struct WindowInput {
    struct Key {
        /// An SDL_Scancode.
        int scancode{0};
        bool down{false};
    };

    bool quit{false};
    std::vector<Key> keys;
    std::vector<Finger> fingers;
    /// The window came back (true) or was minimized (false), as Android does in the background.
    std::optional<bool> shown;
};

/// The target the overlay covers, in pixels: its size, the part no notch or rounded corner
/// covers, and pixels per dp.
struct WindowSurface {
    unsigned width{0};
    unsigned height{0};
    float safeX{0.0F};
    float safeY{0.0F};
    float safeWidth{0.0F};
    float safeHeight{0.0F};
    float dp{1.0F};
};

} // namespace opensu::retro

// sdl_keys — SDL's physical keys as the shell's key codes (input/key_names).
#pragma once

#include <optional>

#include <SDL3/SDL_scancode.h>

namespace opensu::app {

/// The key code for `scancode`, or nothing for a key the shell has no code for.
[[nodiscard]] std::optional<int> keyOf(SDL_Scancode scancode) noexcept;

} // namespace opensu::app

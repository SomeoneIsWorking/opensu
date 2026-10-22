// keyboard_pad — the pad buttons on the on-screen keyboard, the one implementation every field
// shares (search, a folder path, the password).
//
// Regular grid: the D-pad walks the keys, A types the focused key, B deletes and leaves when
// nothing is left, X toggles capitals on a password, Y types a space, Select clears, Start is done.
// Daisy wheel: the left stick picks a petal (`ui::OnScreenKeyboard::stick`, fed by the shell from
// stick-origin directions) and A, B, X and Y type its characters; with no petal picked X deletes,
// Y types a space and B leaves. LB deletes and RB types a space either way, LT and RT step the
// set, D-pad Left and Right move the cursor, Select clears, Start is done.
#pragma once

#include <cstdint>

#include "audio/sdl_sound_player.hpp"
#include "gamepad/event.hpp"
#include "ui/on_screen_keyboard.hpp"

namespace opensu::app {

enum class KeyboardOutcome : std::uint8_t {
    /// The keyboard took the button, or ignored it, and the line is as it was.
    Handled,
    /// The line changed.
    Edited,
    /// The player is done: Done, or Start.
    Confirm,
    /// The player leaves: B with nothing to delete on the grid, B with no petal on the wheel.
    Leave,
};

/// The direction a D-pad button points; Right for any other button.
[[nodiscard]] ui::Direction directionOf(gamepad::Button button) noexcept;

/// Applies `button` to `keyboard`, with iiSU's Navigation sound for a focus or cursor move.
KeyboardOutcome actOnKeyboard(ui::OnScreenKeyboard& keyboard, gamepad::Button button,
                              audio::SdlSoundPlayer& sounds);

} // namespace opensu::app

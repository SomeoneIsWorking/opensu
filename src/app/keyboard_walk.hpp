// keyboard_walk — a pad button on the on-screen keyboard, for the entries that type a line of text
// (a folder path, a password): D-pad walks the keys, A types the focused key, B deletes and leaves
// when nothing is left, Y types a space, Select clears, Start is done. The one implementation, so
// every entry behaves alike.
#pragma once

#include <cstdint>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "ui/search_panel.hpp"

namespace opensu::app {

enum class KeyboardOutcome : std::uint8_t {
    /// The panel took the button (or ignored it).
    Handled,
    /// The player is done: A on Done, or Start.
    Confirm,
    /// B with nothing left to delete.
    Leave,
};

/// The direction a D-pad button points; Right for any other button.
[[nodiscard]] ui::Direction directionOf(gamepad::Button button) noexcept;

/// Applies `button` to `keyboard`, with iiSU's Navigation sound for a focus move. X toggles
/// capitals on a secret entry.
KeyboardOutcome walkKeyboard(ui::SearchPanel& keyboard, gamepad::Button button,
                             audio::SoundPlayer& sounds);

} // namespace opensu::app

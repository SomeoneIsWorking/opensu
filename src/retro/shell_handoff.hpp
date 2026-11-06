// shell_handoff — where the shell does not hear the runner's input (Android, where each is an
// Activity of its own): Guide and Back bring the shell's Activity over the game, whose window it
// lets show through, and the runner draws its own touch pad. Picked in CMakeLists.txt:
// `shell_handoff_android.cpp`; `shell_handoff_none.cpp` elsewhere, where the shell hears Guide
// itself.
#pragma once

namespace opensu::retro::shell_handoff {

/// Whether the runner hands the menu to the shell and draws its own touch pad.
[[nodiscard]] bool active() noexcept;

/// Brings the shell over the game for its menu; false when it could not be asked.
bool openMenu();

} // namespace opensu::retro::shell_handoff

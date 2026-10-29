// shell_handoff — where the shell cannot draw over the runner (Android, where each is an Activity
// of its own): Guide and Back bring the shell's Activity to the front, where its menu opens over
// the paused game, and the runner draws its own touch pad. Picked in CMakeLists.txt:
// `shell_handoff_android.cpp`; `shell_handoff_none.cpp` elsewhere, where the shell hears Guide
// itself and draws over the game.
#pragma once

namespace opensu::retro::shell_handoff {

/// Whether the runner hands the menu to the shell and draws its own touch pad.
[[nodiscard]] bool active() noexcept;

/// Brings the shell to the front for its menu; false when it could not be asked.
bool openMenu();

} // namespace opensu::retro::shell_handoff

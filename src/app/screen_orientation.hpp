// screen_orientation — which way the shell stands, where the system turns the screen for it
// (Android's Activity); elsewhere the window's shape decides and there is nothing to turn.
#pragma once

namespace opensu::app::screen_orientation {

/// Whether this system turns the screen for the shell.
[[nodiscard]] bool turnable() noexcept;

/// The shell rests in portrait or landscape; a game's own Rotate screen returns here.
void stand(bool portrait);

} // namespace opensu::app::screen_orientation

// gamescope_overlay — draws opensu's own window over a running game, as Steam's overlay does.
//
// Gamescope composites a window carrying STEAM_OVERLAY above the focused game, blended by
// _NET_WM_WINDOW_OPACITY and the window's own alpha; STEAM_INPUT_FOCUS hands it keyboard
// focus. All three are read on PropertyNotify, so they change while the window stays mapped.
// The window must be at least as wide as the root and have an ARGB visual
// (FLAG_WINDOW_TRANSPARENT). Gamescope 3.16.29, steamcompmgr.cpp:2689, 3300-3306, 4592-4621.
// It must be presented through Xwayland: the Gamescope WSI layer's Wayland swapchain is opaque
// for it, so CompositorSession turns the layer off for the shell.
// A full-width overlay is painted whenever it is not transparent; without STEAM_INPUT_FOCUS the
// game keeps the keyboard and the pointer (steamcompmgr.cpp:3340, 4632, 5247).
// An overlay is never focused, and a nested Gamescope with no focus unmaps its host window
// (steamcompmgr.cpp:5605, WaylandBackend.cpp:1063), so the window is the overlay only while it
// shows something; otherwise the game's newer windows outrank it (steamcompmgr.cpp:4237).
#pragma once

#include <cstdint>
#include <memory>

#include "display/overlay_mode.hpp"

namespace opensu::session {

class GamescopeOverlay {
  public:
    /// Opens a connection to the X display opensu's window lives on. `window` is its X id.
    /// Throws std::runtime_error when the display cannot be opened.
    explicit GamescopeOverlay(std::uint64_t window);
    ~GamescopeOverlay();

    GamescopeOverlay(const GamescopeOverlay&) = delete;
    GamescopeOverlay& operator=(const GamescopeOverlay&) = delete;

    /// A game is running: the window stays an ordinary app, behind the game's windows.
    void enter();
    /// Shows the window over the game as the overlay, with the keyboard only when `Interactive`
    /// (`Typing` leaves the game the keyboard the on-screen keyboard types into);
    /// `Hidden` makes it an ordinary app again.
    void show(display::OverlayMode mode);
    /// The game's windows are gone: the window is an ordinary app and takes focus back.
    void leave();

  private:
    /// No overlay properties: a focus candidate Gamescope falls back to.
    void ordinary();

    struct Connection;
    std::unique_ptr<Connection> connection_;
};

} // namespace opensu::session

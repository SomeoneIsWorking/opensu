// gamescope_overlay — draws iideck's own window over a running game, as Steam's overlay does.
//
// Gamescope composites a window carrying STEAM_OVERLAY above the focused game, blended by
// _NET_WM_WINDOW_OPACITY and the window's own alpha; STEAM_INPUT_FOCUS hands it keyboard
// focus. All three are read on PropertyNotify, so they change while the window stays mapped.
// The window must be at least as wide as the root and have an ARGB visual
// (FLAG_WINDOW_TRANSPARENT). Gamescope 3.16.29, steamcompmgr.cpp:2689, 3300-3306, 4592-4621.
#pragma once

#include <cstdint>
#include <memory>

namespace iideck::session {

class GamescopeOverlay {
  public:
    /// Opens a connection to the X display iideck's window lives on. `window` is its X id.
    /// Throws std::runtime_error when the display cannot be opened.
    explicit GamescopeOverlay(std::uint64_t window);
    ~GamescopeOverlay();

    GamescopeOverlay(const GamescopeOverlay&) = delete;
    GamescopeOverlay& operator=(const GamescopeOverlay&) = delete;

    /// A game is running: the window becomes the overlay, fully transparent.
    void enter();
    /// Shows or hides the overlay, and gives it the keyboard while shown.
    void setShown(bool shown);
    /// The game's windows are gone: the window is an ordinary app again and takes focus back.
    /// Called only then, because Gamescope focuses the most recently mapped window.
    void leave();

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
};

} // namespace iideck::session

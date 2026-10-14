// gamescope_windows — the windows Gamescope would show, whose processes own them, and which one
// it has focused (`window_focus`).
//
// Gamescope publishes GAMESCOPE_FOCUSABLE_WINDOWS on the root window: one (window, app id, pid)
// triple per window it could focus, the pid found by Gamescope itself, so a window that names no
// _NET_WM_PID still counts. GAMESCOPE_FOCUSED_WINDOW names the focused one in every mode
// (steamcompmgr.cpp:5151-5190, 5493).
#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <sys/types.h>

#include "launch/game_windows.hpp"
#include "window_focus.hpp"

namespace opensu::session {

class GamescopeWindows final : public launch::GameWindows, public FocusSource {
  public:
    /// Opens its own connection to the X display, for the thread that asks.
    /// Throws std::runtime_error when the display cannot be opened.
    GamescopeWindows();
    ~GamescopeWindows() override;

    GamescopeWindows(const GamescopeWindows&) = delete;
    GamescopeWindows& operator=(const GamescopeWindows&) = delete;

    [[nodiscard]] bool anyOwnedBy(const std::vector<pid_t>& pids) override;

    /// The pids owning a focusable window now.
    [[nodiscard]] std::vector<pid_t> owners();

    /// The first call starts watching the root for focus changes; later calls answer only when
    /// one of the two properties changed.
    [[nodiscard]] std::optional<FocusedWindow> takeChange() override;

  private:
    [[nodiscard]] std::vector<FocusableWindow> focusable();
    [[nodiscard]] FocusedWindow focused();

    struct Connection;
    std::unique_ptr<Connection> connection_;
    bool watching_{false};
};

} // namespace opensu::session

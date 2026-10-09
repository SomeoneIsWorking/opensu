// gamescope_windows — which processes own a window Gamescope would show.
//
// Gamescope publishes GAMESCOPE_FOCUSABLE_WINDOWS on the root window: one (window, app id, pid)
// triple per window it could focus, the pid found by Gamescope itself, so a window that names no
// _NET_WM_PID still counts.
#pragma once

#include <memory>
#include <vector>

#include <sys/types.h>

#include "launch/game_windows.hpp"

namespace opensu::session {

class GamescopeWindows final : public launch::GameWindows {
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

  private:
    struct Connection;
    std::unique_ptr<Connection> connection_;
};

} // namespace opensu::session

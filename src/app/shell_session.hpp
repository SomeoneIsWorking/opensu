// shell_session — the session openSU's shell runs in and what it offers: its own compositor started
// around the shell, the windows a launched game shows, the focus, and Steam's own windows kept out
// of sight. One backend per OS, picked in CMakeLists.txt: Gamescope on Linux
// (`shell_session_gamescope.cpp`, the only user of `src/session`); the Windows or macOS desktop,
// whose windows tell when a game shows (`shell_session_desktop.cpp`); Android's Activity tasks
// (`shell_session_android.cpp`).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "config/arguments.hpp"
#include "config/config.hpp"
#include "display/focus.hpp"
#include "launch/game_windows.hpp"

namespace opensu::app {

class ShellSession {
  public:
    ShellSession() = default;
    ShellSession(const ShellSession&) = delete;
    ShellSession& operator=(const ShellSession&) = delete;
    ShellSession(ShellSession&&) = delete;
    ShellSession& operator=(ShellSession&&) = delete;
    virtual ~ShellSession() = default;

    /// Tells the handoff when a game shows a window; null where the session cannot tell.
    [[nodiscard]] virtual launch::GameWindows* gameWindows() noexcept = 0;
    /// Keeps openSU's own Steam's windows out of sight, or lets them show again.
    virtual void hideSteamWindows(bool hides) = 0;
    /// A reader of the window in focus, for `FocusFollow`; null where there is none.
    [[nodiscard]] virtual std::unique_ptr<display::FocusSource> focusSource() = 0;
    /// Whether the shell's window covers its monitor: Gamescope composites a window as the overlay
    /// only when it spans the screen.
    [[nodiscard]] virtual bool coversMonitor() const noexcept = 0;
};

/// The session the shell runs in: inside Gamescope, its windows and focus; on Windows and macOS,
/// the desktop's windows; on Android, its Activity's task; null outside Gamescope on Linux. Throws
/// std::runtime_error when the display cannot be opened.
[[nodiscard]] std::unique_ptr<ShellSession> openShellSession(const config::Config& config);

/// Runs openSU inside a compositor of its own when this OS has one: the run's exit status. Nothing
/// when the shell is to run here as it is.
[[nodiscard]] std::optional<int> runInOwnSession(const config::Config& config,
                                                 const config::Arguments& arguments,
                                                 const std::vector<std::string>& args);

} // namespace opensu::app

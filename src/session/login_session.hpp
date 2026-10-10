// login_session — openSU as the display manager's login session: the top-level Gamescope, and what
// the next login is once it ends.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "compositor_session.hpp"
#include "host/login_session_end.hpp"

namespace opensu::session {

class LoginSession {
  public:
    /// `executablePath` is where the desktop fallback is looked for.
    LoginSession(CompositorSession compositor, host::LoginSessionEnd end,
                 std::vector<std::filesystem::path> executablePath);

    /// Runs the top-level Gamescope with `args` and settles the end. Returns the process exit
    /// status: Gamescope's, or 1 when that was 0 yet the session failed. Returns only when the
    /// desktop fallback could not start, as `startplasma-wayland` otherwise replaces the process.
    int run(const std::vector<std::string>& args);

  private:
    CompositorSession compositor_;
    host::LoginSessionEnd end_;
    std::vector<std::filesystem::path> executablePath_;
};

} // namespace opensu::session

// session — runs opensu inside its own Gamescope.
//
// On a desktop there is no compositor of opensu's own, so it makes one: a nested
// Gamescope at the monitor's size, with opensu as its client. As a login session it is the
// top-level Gamescope on the seat at the native mode. Steam and every game then run inside
// it, and everything of the session ends when Gamescope does.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "gamescope.hpp"

namespace opensu::session {

/// The environment variable that tells the inner opensu which session it belongs to.
inline constexpr const char* sessionVariable = "OPENSU_SESSION";

class CompositorSession {
  public:
    /// `session` names every scope of the run; `gamescope` is the pinned fork's binary
    /// (`Config::gamescope`).
    CompositorSession(std::string session, std::filesystem::path gamescope);

    /// Runs `<this executable> <args>` in a Gamescope sized to `output` (top level when it is
    /// native), in the scope
    /// `<session>-compositor.scope`, and waits for it to end. Then stops every other
    /// scope of the session. Returns Gamescope's exit status, or 1 when it could not
    /// be started, among it a missing `gamescope`. SIGINT and SIGTERM stop the session instead of
    /// killing this process.
    int run(const Output& output, const std::vector<std::string>& args);

    /// Whether the last `run` ended because SIGINT or SIGTERM stopped it (a logout or shutdown),
    /// rather than Gamescope ending by itself.
    [[nodiscard]] bool stopRequested() const noexcept {
        return stopRequested_;
    }

  private:
    /// Stops the scopes of the session that are still up.
    void stopLeftovers() const;

    std::string session_;
    std::filesystem::path gamescope_;
    bool stopRequested_{false};
};

} // namespace opensu::session

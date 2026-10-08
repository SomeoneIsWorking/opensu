// session — runs iideck inside its own Gamescope.
//
// On a desktop there is no compositor of iideck's own, so it makes one: a nested
// Gamescope at the monitor's size, with iideck as its client. Steam and every game
// then run inside it, and everything of the session ends when Gamescope does.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "gamescope.hpp"

namespace iideck::session {

/// The environment variable that tells the inner iideck which session it belongs to.
inline constexpr const char* sessionVariable = "IIDECK_SESSION";

class NestedSession {
  public:
    /// `session` names every scope of the run; `gamescope` is the pinned fork's binary
    /// (`Config::gamescope`).
    NestedSession(std::string session, std::filesystem::path gamescope);

    /// Runs `<this executable> <args>` in a Gamescope sized to `output`, in the scope
    /// `<session>-compositor.scope`, and waits for it to end. Then stops every other
    /// scope of the session. Returns Gamescope's exit status, or 1 when it could not
    /// be started, among it a missing `gamescope`. SIGINT and SIGTERM stop the session instead of
    /// killing this process.
    int run(const Output& output, const std::vector<std::string>& args);

  private:
    /// Stops the scopes of the session that are still up.
    void stopLeftovers() const;

    std::string session_;
    std::filesystem::path gamescope_;
};

} // namespace iideck::session

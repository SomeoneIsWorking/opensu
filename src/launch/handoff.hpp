// launch — starts a game and hides the shell until it exits.
#pragma once

#include <functional>
#include <string>

#include "library/game.hpp"

namespace iideck::launch {

/// A launch that cannot be attempted.
struct Error {
    std::string id;
    std::string reason;

    [[nodiscard]] std::string message() const;
};

/// Starts games. One at a time: a second launch is refused while one is running.
class Handoff {
  public:
    /// The command was started but could not be waited on.
    static constexpr int kWatchFailed = -1;

    /// Starts a game, calls `hide`, waits for the game to finish, then calls
    /// `show`. Blocks until then, so callers run it off the main thread.
    ///
    /// The wait covers both the spawned child and the process table, because
    /// Steam and Legendary hand off to a different process and waiting on the
    /// child alone would show the shell while the game is still loading.
    static bool start(const library::Game& game, const std::function<void()>& hide,
                      const std::function<void()>& show, std::string& failure);

    /// True when any running process's command line contains `hint`. Every
    /// source records a hint that is unique to its own game: the Wine prefix
    /// path for Steam, the install folder for the others.
    [[nodiscard]] static bool processMatches(const std::string& hint);
};

} // namespace iideck::launch
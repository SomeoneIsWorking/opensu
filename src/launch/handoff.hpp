// launch — starts a game and hides the shell until it exits.
#pragma once

#include <chrono>
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
    /// How long a game has to appear in the process table after the launcher
    /// starts. A launcher that hands off and exits, which Steam's does, gives up
    /// on its child long before this; the bound is what stops a launcher that
    /// stays alive from holding the shell hidden for the whole watch timeout
    /// while nothing is running.
    static constexpr std::chrono::seconds startTimeout{std::chrono::minutes{3}};

    /// How long a game that did appear may keep running before the shell gives
    /// up on it. A game left running overnight must not hide the shell forever.
    static constexpr std::chrono::seconds watchTimeout{std::chrono::hours{12}};

    /// Starts a game, calls `hide`, waits for the game to finish, then calls
    /// `show`. Blocks until then, so callers run it off the main thread.
    ///
    /// Waiting is a two-step state, not a wait on the child. A launcher may hand
    /// off to a different process and exit immediately, so the child exiting
    /// means nothing; and a launcher that IS the long-lived process (Steam's own
    /// client) never exits, so waiting on the child would never end either.
    /// What matters is the game itself, so the wait is for the game to appear in
    /// the process table, then for it to leave.
    static bool start(const library::Game& game, const std::function<void()>& hide,
                      const std::function<void()>& show, std::string& failure);

    /// True when any running process's command line contains `hint`. Every
    /// source records a hint that is unique to its own game: the Wine prefix
    /// path for Steam, the install folder for the others.
    [[nodiscard]] static bool processMatches(const std::string& hint);
};

} // namespace iideck::launch
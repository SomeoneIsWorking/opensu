// game_launch — the one game the shell runs at a time: started on a thread of its own through the
// handoff, whether one is launching or running for any reader, cancelling or closing it from a
// menu, and its end told to the loop through the mailbox (the toast `game_end` words).
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "launch/handoff.hpp"
#include "launch_mailbox.hpp"
#include "library/game.hpp"
#include "retro_session.hpp"

namespace opensu::app {

class GameLaunch {
  public:
    GameLaunch(launch::Handoff& handoff, LaunchMailbox& mailbox, RetroSession& retro) noexcept
        : handoff_{handoff}, mailbox_{mailbox}, retro_{retro} {
    }

    /// Starts `game` with each "NAME=value" of `environment` added to its environment; false when
    /// a game is already launching or running. Main loop only.
    bool start(library::Game game, std::vector<std::string> environment);
    /// Whether a game is launching or running. Any thread.
    [[nodiscard]] bool running() const;
    /// Whether `game` is the one launching or running.
    [[nodiscard]] bool runs(const library::Game& game) const;
    /// The game started last. Main loop only.
    [[nodiscard]] const library::Game& game() const noexcept {
        return game_;
    }
    /// Abandons a launch whose game has not appeared yet, such as one waiting on a Steam update.
    void cancel();
    /// Ends the running game, from a menu.
    void close();

  private:
    launch::Handoff& handoff_;
    LaunchMailbox& mailbox_;
    RetroSession& retro_;
    library::Game game_;
    mutable std::mutex mutex_;
    bool running_{false};
};

} // namespace opensu::app

// game_launch — the one game the shell runs at a time: started on a thread of its own through the
// handoff, whether one is launching or running for any reader, cancelling or closing it from a
// menu, and its end told to the loop through the mailbox (the toast `game_end` words) and
// `takeEnded`. Destroying
// it closes the running game and joins that thread, so the handoff, mailbox and runner session
// the thread uses must outlive it.
#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "launch/handoff.hpp"
#include "launch_mailbox.hpp"
#include "library/game.hpp"
#include "retro_session.hpp"

namespace opensu::app {

class GameLaunch {
  public:
    /// Turns a conversion into the zip to start, on the launch's thread; nothing, with `error`.
    using Convert = std::function<std::optional<std::filesystem::path>(
        const library::RomConversion& conversion, std::string& error)>;

    GameLaunch(launch::Handoff& handoff, LaunchMailbox& mailbox, RetroSession& retro) noexcept
        : handoff_{handoff}, mailbox_{mailbox}, retro_{retro} {
    }
    GameLaunch(const GameLaunch&) = delete;
    GameLaunch& operator=(const GameLaunch&) = delete;
    /// Closes the running game, as from a menu, and waits until its end has been told.
    ~GameLaunch();

    /// Starts `game` with each "NAME=value" of `environment` added to its environment; false when
    /// a game is already launching or running. `convert` serves a game whose launch has a
    /// conversion. Main loop only.
    bool start(library::Game game, std::vector<std::string> environment, Convert convert = {});
    /// Whether a game is launching or running. Any thread.
    [[nodiscard]] bool running() const;
    /// Whether `game` is the one launching or running.
    [[nodiscard]] bool runs(const library::Game& game) const;
    /// The game started last. Main loop only.
    [[nodiscard]] const library::Game& game() const noexcept {
        return game_;
    }
    /// The game whose launch ended since last asked, once. Main loop only.
    [[nodiscard]] std::optional<library::Game> takeEnded();
    /// Abandons a launch whose game has not appeared yet, such as one waiting on a Steam update;
    /// its end is told as cancelled.
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
    bool cancelled_{false};
    std::optional<library::Game> ended_;
    /// The launch's thread; last, so it is joined before anything it reads goes.
    std::jthread thread_;
};

} // namespace opensu::app

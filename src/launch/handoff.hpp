// launch — starts a game inside an instance and hides the shell until it is over.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "game_windows.hpp"
#include "instance.hpp"
#include "launch_progress.hpp"
#include "library/game.hpp"
#include "steam_gate.hpp"

namespace iideck::launch {

/// A launch that cannot be attempted.
struct Error {
    std::string id;
    std::string reason;

    [[nodiscard]] std::string message() const;
};

/// Starts games, one at a time. A Steam game is handed to the Steam client iideck
/// owns, which runs it in its own scope; any other game runs in an Instance of its own.
class Handoff {
  public:
    /// How long a game has to appear in the process table after the launcher
    /// starts, and how long a Steam launch waits for the client to be ready. A
    /// launcher that stays alive without ever running the game must not hold the
    /// launch open for the whole watch timeout.
    static constexpr std::chrono::seconds startTimeout{std::chrono::minutes{3}};

    /// `executablePath` is where a launch's program is looked up; `session` names
    /// the scopes (`<session>-game-N.scope`); `steam` is the client Steam games wait for;
    /// `windows` tells when a game shows a window, or is null where there is no display to
    /// watch, and a game then counts as shown once it runs.
    Handoff(std::vector<std::filesystem::path> executablePath, std::string session,
            SteamGate& steam, GameWindows* windows);

    /// What a launch tells its caller, from the thread running start().
    struct Hooks {
        /// The game shows a window.
        std::function<void()> hide;
        /// The game that `hide` announced is over.
        std::function<void()> show;
        /// How the launch is getting on until then; called on each change.
        std::function<void(const LaunchProgress&)> progress;
    };

    /// Starts a game, calls `hide` once it shows a window, waits for it to finish,
    /// then calls `show`. Blocks until then, so callers run it off the main thread. Each
    /// "NAME=value" of `environment` is added to a game iideck starts itself; a
    /// Steam game gets the client's environment.
    ///
    /// A Steam game first waits for the client to be ready (cancellable by
    /// forceClose()), then runs `steam -applaunch`; the client is left running when
    /// the game ends. Anything else runs directly in an Instance, stopped when the
    /// game leaves.
    ///
    /// The wait is two phases, not a wait on the child: a launcher may hand off
    /// and exit at once. The game appears and shows a window, then leaves. It also ends
    /// when the instance is gone, or when forceClose() is called. The game must appear
    /// within startTimeout, a timeout that does not run while Steam is updating it or
    /// working through its launch; once it runs, it may load for as long as it likes.
    ///
    /// A Steam game is followed through Steam's own launch: its tasks are the progress,
    /// its error is the failure, and the game is over once Steam says it no longer runs and
    /// none of its processes remain. A game Steam already runs is returned to, not
    /// launched again.
    bool start(const library::Game& game, const Hooks& hooks,
               const std::vector<std::string>& environment, std::string& failure);

    /// SIGKILLs everything the current launch started: the instance, or a Steam
    /// game's whole process tree (the client stays). Safe from any thread; start()
    /// then returns promptly and calls `show`.
    void forceClose();

  private:
    /// How a launch got on.
    enum class Begun : std::uint8_t { Started, Cancelled, Failed };
    /// How the wait for the game's window ended.
    enum class Shown : std::uint8_t { Window, Forced, Failed };
    /// Reports a progress line once per change.
    using Report = std::function<void(const LaunchProgress&)>;

    /// Asks the Steam client to run the game, once it is ready. `baseline` is Steam's last
    /// launch action for the game before this one.
    Begun beginSteam(const library::Game& game, const Hooks& hooks, int& baseline,
                     std::string& failure);
    /// Starts the game in an Instance of its own.
    Begun beginScope(const library::Game& game, const std::vector<std::string>& environment,
                     std::string& failure);

    /// Phase one for a Steam game: Steam's launch, then the game's window.
    Shown awaitSteamWindow(const library::Game& game, int baseline, const Report& report,
                           std::string& failure);
    /// Phase one for a game in an Instance: it appears, then shows a window.
    Shown awaitScopeWindow(const library::Game& game, const Report& report, std::string& failure);
    /// Phase two: whether the game is still there.
    [[nodiscard]] bool stillRunning(const library::Game& game, bool viaSteam);

    /// Sleeps one poll interval, or less when forceClose() wakes it.
    void pause();

    std::vector<std::filesystem::path> executablePath_;
    std::string session_;
    SteamGate& steam_;
    GameWindows* windows_;
    Instance instance_;
    std::atomic<unsigned> launches_{0};
    std::atomic<bool> forced_{false};
    std::mutex wakeMutex_;
    std::condition_variable wake_;
    /// The running Steam game's hint, empty when none; forceClose() reads it.
    std::mutex hintMutex_;
    std::string steamHint_;
};

} // namespace iideck::launch

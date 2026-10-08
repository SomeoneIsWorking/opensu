// launch — starts a game inside an instance and hides the shell until it is over.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "instance.hpp"
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
    /// shell hidden for the whole watch timeout.
    static constexpr std::chrono::seconds startTimeout{std::chrono::minutes{3}};

    /// `executablePath` is where a launch's program is looked up; `session` names
    /// the scopes (`<session>-game-N.scope`); `steam` is the client Steam games wait for.
    Handoff(std::vector<std::filesystem::path> executablePath, std::string session,
            SteamGate& steam);

    /// Starts a game, calls `hide`, waits for the game to finish, then calls
    /// `show`. Blocks until then, so callers run it off the main thread. Each
    /// "NAME=value" of `environment` is added to a game iideck starts itself; a
    /// Steam game gets the client's environment.
    ///
    /// A Steam game first waits for the client to be ready (cancellable by
    /// forceClose()), then runs `steam -applaunch`; the client is left running when
    /// the game ends. Anything else runs directly in an Instance, stopped when the
    /// game leaves.
    ///
    /// The wait is two phases, not a wait on the child: a launcher may hand off
    /// and exit at once. The game appears in the process table, then leaves. It
    /// also ends when the instance is gone, or when forceClose() is called.
    bool start(const library::Game& game, const std::function<void()>& hide,
               const std::function<void()>& show, const std::vector<std::string>& environment,
               std::string& failure);

    /// SIGKILLs everything the current launch started: the instance, or a Steam
    /// game's whole process tree (the client stays). Safe from any thread; start()
    /// then returns promptly and calls `show`.
    void forceClose();

  private:
    /// How a launch got on.
    enum class Begun { Started, Cancelled, Failed };

    /// Asks the Steam client to run the game, once it is ready.
    Begun beginSteam(const library::Game& game, std::string& failure);
    /// Starts the game in an Instance of its own.
    Begun beginScope(const library::Game& game, const std::vector<std::string>& environment,
                     std::string& failure);

    /// Sleeps one poll interval, or less when forceClose() wakes it.
    void pause();

    std::vector<std::filesystem::path> executablePath_;
    std::string session_;
    SteamGate& steam_;
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

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

#include "desktop_steam.hpp"
#include "gamescope.hpp"
#include "instance.hpp"
#include "library/game.hpp"

namespace iideck::launch {

/// A launch that cannot be attempted.
struct Error {
    std::string id;
    std::string reason;

    [[nodiscard]] std::string message() const;
};

/// Starts games, one at a time, each inside an Instance the shell owns.
class Handoff {
  public:
    /// How long a game has to appear in the process table after the launcher
    /// starts. A launcher that stays alive without ever running the game must
    /// not hold the shell hidden for the whole watch timeout.
    static constexpr std::chrono::seconds startTimeout{std::chrono::minutes{3}};

    /// `home` is where a desktop Steam client records its pid; `executablePath` is
    /// where a launch's program is looked up.
    Handoff(std::filesystem::path home, std::vector<std::filesystem::path> executablePath);

    /// Starts a game, calls `hide`, waits for the game to finish, then calls
    /// `show`. Blocks until then, so callers run it off the main thread.
    ///
    /// Outside Gamescope the game runs in a nested Gamescope window sized to
    /// `output`; inside one it runs directly. Either way it runs in an Instance,
    /// which is stopped when the game leaves.
    ///
    /// The wait is two phases, not a wait on the child: a launcher may hand off
    /// and exit at once. The game appears in the process table, then leaves. It
    /// also ends when the instance is gone, or when forceClose() is called.
    bool start(const library::Game& game, const Output& output, bool insideGamescope,
               const std::function<void()>& hide, const std::function<void()>& show,
               std::string& failure);

    /// SIGKILLs everything the current launch started. Safe from any thread;
    /// start() then returns promptly and calls `show`.
    void forceClose();

    /// True when any running process's command line contains `hint`. Every
    /// source records a hint that is unique to its own game: the Wine prefix
    /// path for Steam, the install folder for the others.
    [[nodiscard]] static bool processMatches(const std::string& hint);

  private:
    /// Sleeps one poll interval, or less when forceClose() wakes it.
    void pause();

    DesktopSteam steam_;
    std::vector<std::filesystem::path> executablePath_;
    Instance instance_;
    std::atomic<unsigned> launches_{0};
    std::atomic<bool> forced_{false};
    std::mutex wakeMutex_;
    std::condition_variable wake_;
};

} // namespace iideck::launch

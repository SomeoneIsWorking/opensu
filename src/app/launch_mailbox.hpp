// launch_mailbox — what the launch and sign-in threads hand the main loop: whether a game runs, the
// launch's latest progress and a toast. The threads only post here; the loop takes and applies, as
// the window and the shell belong to it.
#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

#include "launch/launch_progress.hpp"

namespace opensu::app {

class LaunchMailbox {
  public:
    struct Toast {
        std::string text{};
        bool error{false};
    };

    // Any thread.
    void postGameRunning(bool running) noexcept {
        gameRunning_.store(running);
    }
    void postProgress(const launch::LaunchProgress& progress);
    /// A later toast replaces one not yet taken.
    void postToast(std::string text, bool error);

    // The loop.
    [[nodiscard]] bool gameRunning() const noexcept {
        return gameRunning_.load();
    }
    [[nodiscard]] std::optional<launch::LaunchProgress> takeProgress();
    [[nodiscard]] std::optional<Toast> takeToast();

  private:
    std::atomic<bool> gameRunning_{false};
    std::mutex mutex_;
    std::optional<launch::LaunchProgress> progress_;
    std::optional<Toast> toast_;
};

} // namespace opensu::app

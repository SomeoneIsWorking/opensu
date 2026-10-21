#include "game_launch.hpp"

#include <thread>
#include <utility>

#include "game_end.hpp"
#include "lucent/log.h"

namespace opensu::app {

bool GameLaunch::running() const {
    const std::lock_guard lock{mutex_};
    return running_;
}

bool GameLaunch::runs(const library::Game& game) const {
    const std::lock_guard lock{mutex_};
    return running_ && game_.id == game.id;
}

bool GameLaunch::start(library::Game game, std::vector<std::string> environment) {
    {
        const std::lock_guard lock{mutex_};
        if (running_) {
            return false;
        }
        running_ = true;
        cancelled_ = false;
        game_ = game;
    }
    // The thread owns a copy because it outlives this call.
    std::thread{[this, copy = std::move(game), environment = std::move(environment)] {
        std::string failure;
        int exitStatus = -1;
        // The window belongs to the loop's thread, so the handoff thread only raises flags and the
        // loop does the work.
        const launch::Handoff::Hooks hooks{.hide =
                                               [this] {
                                                   mailbox_.postGameRunning(true);
                                               },
                                           .show =
                                               [this] {
                                                   mailbox_.postGameRunning(false);
                                               },
                                           .progress =
                                               [this](const launch::LaunchProgress& progress) {
                                                   mailbox_.postProgress(progress);
                                               },
                                           .ended =
                                               [&exitStatus](int status) {
                                                   exitStatus = status;
                                               }};
        handoff_.start(copy, hooks, environment, failure);
        bool cancelled = false;
        {
            const std::lock_guard lock{mutex_};
            running_ = false;
            cancelled = cancelled_;
        }
        const EndNotice notice = noticeOf(GameEnd{.title = copy.title,
                                                  .failure = failure,
                                                  .exitStatus = exitStatus,
                                                  .runnerError = retro_.end(),
                                                  .cancelled = cancelled});
        lucent::info("launch", "{} is over: {}", copy.title, notice.text);
        mailbox_.postToast(notice.text, notice.error);
    }}.detach();
    return true;
}

void GameLaunch::cancel() {
    {
        const std::lock_guard lock{mutex_};
        if (!running_) {
            return;
        }
        cancelled_ = true;
    }
    lucent::info("launch", "cancelling the launch of {}", game_.title);
    handoff_.forceClose();
}

void GameLaunch::close() {
    lucent::info("launch", "closing {} from a menu", game_.title);
    handoff_.forceClose();
}

} // namespace opensu::app

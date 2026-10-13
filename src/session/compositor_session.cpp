#include "compositor_session.hpp"

#include <chrono>
#include <csignal>
#include <ctime>
#include <utility>

#include <pthread.h>

#include "launch/command.hpp"
#include "launch/instance.hpp"
#include "lucent/log.h"

namespace opensu::session {
namespace {

namespace fs = std::filesystem;

constexpr auto stopWait = std::chrono::milliseconds{60000};
constexpr long pollNanoseconds = 200'000'000;

/// Takes a pending signal of `signals`, waiting at most `wait`.
bool takeSignal(const sigset_t& signals, const timespec& wait) {
    return sigtimedwait(&signals, nullptr, &wait) > 0;
}

} // namespace

CompositorSession::CompositorSession(std::string session, fs::path gamescope,
                                     host::DesktopRequest desktopRequest, RestartLimit restarts)
    : session_{std::move(session)}, gamescope_{std::move(gamescope)},
      desktopRequest_{std::move(desktopRequest)}, restarts_{restarts} {
}

void CompositorSession::stopLeftovers() const {
    // systemctl takes a glob, which reaches the steam and game scopes by name.
    (void)launch::runCommand("systemctl", {"--user", "stop", session_ + "-*"}, stopWait);
}

int CompositorSession::run(const Output& output, const std::vector<std::string>& args) {
    stopRequested_ = false;
    if (launch::resolveExecutable(gamescope_.string(), {}).empty()) {
        lucent::error(
            "session",
            "the Gamescope fork is not at '{}'; opensu was built without it. Rebuild with "
            "-DOPENSU_BUILD_GAMESCOPE=ON (the default)",
            gamescope_.string());
        return 1;
    }
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) {
        lucent::error("session", "cannot find this executable: {}", ec.message());
        return 1;
    }

    // Blocked for the whole run (a started Gamescope does not inherit the mask), so a signal is
    // taken here and ends the session through the scopes, rather than killing this process and
    // leaving them running.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    sigset_t previous;
    pthread_sigmask(SIG_BLOCK, &signals, &previous);

    RestartBudget budget{restarts_};
    int status = 1;
    for (;;) {
        status = runOnce(output, self.string(), args, signals);
        stopLeftovers();
        if (status < 0) {
            status = 1;
            break;
        }
        if (!restartAfter(status, budget, signals)) {
            break;
        }
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    lucent::info("session", "{} ended", session_);
    return status;
}

int CompositorSession::runOnce(const Output& output, const std::string& self,
                               const std::vector<std::string>& args, const sigset_t& signals) {
    launch::Instance compositor;
    std::string failure;
    if (!compositor.start(session_ + "-compositor.scope", gamescope_.string(),
                          gamescopeArgs(output, self, args), failure,
                          {std::string{sessionVariable} + "=" + session_})) {
        lucent::error("session", "{}", failure);
        return -1;
    }
    if (output.native()) {
        lucent::info("session", "{} runs in a top-level gamescope", session_);
    } else {
        lucent::info("session", "{} runs in a nested gamescope at {}x{}", session_, output.width,
                     output.height);
    }

    // Gamescope's own exit ends the wait even when something it started is still in the scope.
    const timespec poll{0, pollNanoseconds};
    while (compositor.running() && compositor.exitStatus() < 0) {
        if (takeSignal(signals, poll)) {
            lucent::info("session", "stopping {}", session_);
            stopRequested_ = true;
            compositor.stop();
        }
    }
    compositor.stop();
    return compositor.exitStatus() >= 0 ? compositor.exitStatus() : 1;
}

bool CompositorSession::restartAfter(int status, RestartBudget& budget, const sigset_t& signals) {
    if (stopRequested_ || status == 0) {
        return false;
    }
    // A stop signal that came while the leftovers were stopped is still a requested end.
    if (takeSignal(signals, timespec{0, 0})) {
        stopRequested_ = true;
        return false;
    }
    if (desktopRequest_.pending()) {
        lucent::info("session", "gamescope ended (status {}) on the way to the desktop", status);
        return false;
    }
    const RestartLimit& limit = budget.limit();
    if (!budget.spend(RestartBudget::Clock::now())) {
        lucent::error("session",
                      "gamescope ended abnormally (status {}) after {} restarts within {} s; "
                      "giving up",
                      status, limit.restarts, limit.window.count());
        return false;
    }
    lucent::warn("session", "gamescope ended abnormally (status {}); starting a fresh {}", status,
                 session_);
    return true;
}

} // namespace opensu::session

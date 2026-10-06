#include "nested_session.hpp"

#include <chrono>
#include <csignal>
#include <ctime>
#include <thread>
#include <utility>

#include <pthread.h>

#include "launch/command.hpp"
#include "launch/instance.hpp"
#include "lucent/log.h"

namespace iideck::session {
namespace {

namespace fs = std::filesystem;

constexpr auto stopWait = std::chrono::milliseconds{60000};
constexpr long pollNanoseconds = 200'000'000;

} // namespace

NestedSession::NestedSession(std::string session, std::vector<fs::path> executablePath)
    : session_{std::move(session)}, executablePath_{std::move(executablePath)} {
}

void NestedSession::stopLeftovers() const {
    // systemctl takes a glob, which reaches the steam and game scopes by name.
    (void)launch::runCommand("systemctl", {"--user", "stop", session_ + "-*"}, stopWait);
}

int NestedSession::run(const Output& output, const std::vector<std::string>& args) {
    const fs::path gamescope = launch::resolveExecutable("gamescope", executablePath_);
    if (gamescope.empty()) {
        lucent::error("session", "gamescope is not installed. Fedora: sudo dnf install gamescope; "
                                 "Debian/Ubuntu: sudo apt install gamescope; "
                                 "Arch: sudo pacman -S gamescope");
        return 1;
    }
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) {
        lucent::error("session", "cannot find this executable: {}", ec.message());
        return 1;
    }

    launch::Instance compositor;
    std::string failure;
    if (!compositor.start(session_ + "-compositor.scope", gamescope.string(),
                          gamescopeArgs(output, self.string(), args), failure,
                          {std::string{sessionVariable} + "=" + session_})) {
        lucent::error("session", "{}", failure);
        return 1;
    }
    lucent::info("session", "{} runs in a nested gamescope at {}x{}", session_, output.width,
                 output.height);

    // Blocked only after the start, so the children do not inherit the mask. A signal
    // is then taken here and ends the session through the scopes, rather than
    // killing this process and leaving them running.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    sigset_t previous;
    pthread_sigmask(SIG_BLOCK, &signals, &previous);

    const timespec poll{0, pollNanoseconds};
    while (compositor.running()) {
        if (sigtimedwait(&signals, nullptr, &poll) > 0) {
            lucent::info("session", "stopping {}", session_);
            compositor.stop();
        }
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);

    stopLeftovers();
    lucent::info("session", "{} ended", session_);
    return compositor.exitStatus() >= 0 ? compositor.exitStatus() : 1;
}

} // namespace iideck::session

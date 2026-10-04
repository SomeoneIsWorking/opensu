#include "handoff.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "library/game.hpp"
#include "lucent/log.h"

namespace iideck::launch {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::system_clock;

/// How often the process table is consulted while waiting.
constexpr auto pollInterval = std::chrono::milliseconds{750};

/// The exit status a child uses when execvp failed. 127 is what a shell reports
/// for a command it could not run.
constexpr int kExecFailed = 127;

/// The argument vector for execvp, owning the strings the pointers refer to.
///
/// The two must not be separable: a vector<char*> built alongside a local vector
/// of strings dangles the moment that local dies, and execvp then reads freed
/// memory. Ownership is therefore explicit, and the whole thing is built in the
/// parent before the fork, because allocating in a forked child of a threaded
/// process is not safe.
class Argv {
  public:
    Argv(const std::string& program, const std::vector<std::string>& args) {
        owned_.reserve(args.size() + 1);
        owned_.push_back(program);
        for (const std::string& arg : args) {
            owned_.push_back(arg);
        }
        pointers_.reserve(owned_.size() + 1);
        for (std::string& value : owned_) {
            pointers_.push_back(value.data());
        }
        pointers_.push_back(nullptr);
    }

    [[nodiscard]] char** data() noexcept { return pointers_.data(); }
    [[nodiscard]] const std::string& program() const noexcept { return owned_.front(); }

  private:
    std::vector<std::string> owned_;
    std::vector<char*> pointers_;
};

} // namespace

std::string Error::message() const {
    return "cannot launch " + id + ": " + reason;
}

bool Handoff::processMatches(const std::string& hint) {
    if (hint.empty()) {
        return false;
    }
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{"/proc", ec}) {
        if (ec) {
            return false;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() < '0' || name.front() > '9') {
            continue;
        }
        std::ifstream cmdline{entry.path() / "cmdline", std::ios::binary};
        if (!cmdline) {
            // The process exited between the readdir and the read.
            continue;
        }
        std::string text{std::istreambuf_iterator<char>{cmdline}, std::istreambuf_iterator<char>{}};
        if (text.find(hint) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool Handoff::start(const library::Game& game, const std::function<void()>& hide,
                    const std::function<void()>& show, std::string& failure) {
    if (game.launch.empty()) {
        failure = "no launch command for " + game.title;
        return false;
    }
    if (game.processHint.empty()) {
        // Without a hint there is nothing to watch, and the two-phase wait would
        // sit here for the whole appearance bound and then report a game that is
        // running fine as having failed to start. Refusing is the honest answer:
        // this source cannot support a handoff.
        failure = std::string{library::label(game.source)} + " cannot tell when " +
                 game.title + " is running";
        return false;
    }

    // Built before the fork: the child must only call async-signal-safe functions,
    // and this owns the strings execvp will read.
    Argv args{game.launch.program, game.launch.args};

    // The game must outlive the shell: it gets its own session so a shell exit
    // or a controlling-terminal hangup cannot reach it.
    const pid_t pid = fork();
    if (pid < 0) {
        failure = std::string{"fork failed: "} + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        // Child: a new session, then exec. Nothing is allocated here.
        setsid();
        execvp(args.program().c_str(), args.data());
        // exec failed. The parent turns this status into a named failure.
        std::_Exit(kExecFailed);
    }

    // Hidden before the spawn, not after: a game that opens its window
    // immediately would otherwise appear over a shell that is still up, and
    // Steam's client is running long before the game it starts.
    if (hide) {
        hide();
    }
    lucent::info("launch", "started {} (pid {})", game.title, pid);

    // The child is reaped throughout: a launcher that exits must not be left as
    // a zombie, and a zombie is worse than a lost exit status.
    bool childExited = false;
    int status = 0;
    const auto reap = [&childExited, &status, pid] {
        if (childExited) {
            return;
        }
        const pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid || (done < 0 && errno != EINTR)) {
            childExited = true;
        }
    };

    // Waiting is two phases, not a wait on the child. What matters is the game,
    // and the child says nothing useful about a successful launch: a launcher
    // that hands off exits immediately, and one that IS the long-lived process
    // never exits.
    //
    // It does say something about a failed one. execvp returning 127 means the
    // program never ran, so there is nothing to wait for and the wait must end
    // at once rather than sitting out the appearance bound on a launch that can
    // never succeed.
    const Clock::time_point startDeadline = Clock::now() + Handoff::startTimeout;
    bool running = false;
    while (!running) {
        reap();
        if (processMatches(game.processHint)) {
            running = true;
            break;
        }
        if (childExited && WIFEXITED(status) && WEXITSTATUS(status) == kExecFailed) {
            failure = "could not start " + game.launch.program;
            reap();
            if (show) {
                show();
            }
            return false;
        }
        // Every exit from here leaves through this, so the child is reaped on
        // each one rather than only on success.
        if (Clock::now() >= startDeadline) {
            lucent::warn("launch", "{} did not appear within {}", game.title,
                         Handoff::startTimeout);
            if (show) {
                show();
            }
            failure = game.title + " did not start";
            reap();
            return false;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    lucent::info("launch", "{} is running", game.title);

    // Phase two: the game leaves.
    const Clock::time_point runDeadline = Clock::now() + Handoff::watchTimeout;
    while (processMatches(game.processHint)) {
        reap();
        if (Clock::now() >= runDeadline) {
            lucent::warn("launch", "{} is still running after {}", game.title,
                         Handoff::watchTimeout);
            break;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    reap();
    if (show) {
        show();
    }
    lucent::info("launch", "{} finished", game.title);
    return true;
}

} // namespace iideck::launch
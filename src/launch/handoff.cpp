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

#include "lucent/log.h"

namespace iideck::launch {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::system_clock;

/// How often the process table is consulted while a game runs.
constexpr auto pollInterval = std::chrono::seconds{2};

/// Bounds the wait, so a game whose process cannot be recognised cannot leave
/// the shell hidden forever.
constexpr auto watchTimeout = std::chrono::hours{12};

/// Builds the argument vector, exec-safe.
std::vector<char*> argv(const std::string& program, const std::vector<std::string>& args)
{
    std::vector<std::string> owned;
    owned.push_back(program);
    for (const std::string& arg : args) {
        owned.push_back(arg);
    }
    std::vector<char*> out;
    out.reserve(owned.size() + 1);
    for (std::string& value : owned) {
        out.push_back(value.data());
    }
    out.push_back(nullptr);
    return out;
}

} // namespace

std::string Error::message() const
{
    return "cannot launch " + id + ": " + reason;
}

bool Handoff::processMatches(const std::string& hint)
{
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
    const std::function<void()>& show, std::string& failure)
{
    if (game.launch.empty()) {
        failure = "no launch command for " + game.title;
        return false;
    }

    // The game must outlive the shell: it gets its own session so a shell exit
    // or a controlling-terminal hangup cannot reach it.
    pid_t pid = fork();
    if (pid < 0) {
        failure = std::string{"fork failed: "} + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        // Child: a new session, then exec. Failure is reported through a pipe
        // rather than by exiting quietly.
        setsid();
        std::vector<char*> args = argv(game.launch.program, game.launch.args);
        execvp(args.front(), args.data());
        std::_Exit(127);
    }

    lucent::info("launch", "started {} (pid {})", game.title, pid);
    if (hide) {
        hide();
    }

    const Clock::time_point deadline = Clock::now() + watchTimeout;
    bool childExited = false;
    bool seenInTable = false;
    int status = 0;

    while (true) {
        if (!childExited) {
            pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid) {
                childExited = true;
            } else if (done < 0) {
                childExited = true;
            }
        }

        const bool inTable = processMatches(game.processHint);
        if (inTable) {
            seenInTable = true;
        } else if (seenInTable) {
            break;
        } else if (childExited) {
            // The child is gone and nothing matches, so the game is over whether
            // or not it was ever seen in the table.
            break;
        }

        if (Clock::now() > deadline) {
            lucent::warn("launch", "timed out waiting for {} to exit", game.title);
            break;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    if (show) {
        show();
    }

    if (childExited && WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        failure = "could not start " + game.launch.program;
        return false;
    }
    lucent::info("launch", "{} finished", game.title);
    return true;
}

} // namespace iideck::launch
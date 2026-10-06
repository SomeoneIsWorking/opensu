#include "instance.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "lucent/log.h"

namespace iideck::launch {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr auto scopeWait = std::chrono::seconds{5};
constexpr auto reapWait = std::chrono::seconds{10};
constexpr auto step = std::chrono::milliseconds{5};

/// Runs `systemctl --user <args>` with its output discarded. Returns its exit
/// status, or -1 when it could not be run.
int systemctl(const std::vector<std::string>& args) {
    std::vector<std::string> full{"--user"};
    full.insert(full.end(), args.begin(), args.end());
    Argv argv{"systemctl", full};

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = -1;
    const int spawned = posix_spawnp(&pid, "systemctl", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0) {
        lucent::warn("launch", "cannot run systemctl: {}", std::strerror(spawned));
        return -1;
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/// True when /proc/<pid>/cgroup already places the process in `unit`.
bool inScope(pid_t pid, const std::string& unit) {
    std::ifstream in{"/proc/" + std::to_string(pid) + "/cgroup"};
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(unit) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

Instance::~Instance() {
    stop();
}

bool Instance::findExecutable(const std::string& program, const std::vector<fs::path>& path) {
    if (program.empty()) {
        return false;
    }
    const auto executable = [](const fs::path& candidate) {
        std::error_code ec;
        return fs::is_regular_file(candidate, ec) && access(candidate.c_str(), X_OK) == 0;
    };
    if (program.find('/') != std::string::npos) {
        return executable(program);
    }
    return std::ranges::any_of(path, [&](const fs::path& dir) {
        return executable(dir / program);
    });
}

bool Instance::start(const std::string& unit, const std::string& program,
                     const std::vector<std::string>& args, std::string& failure) {
    const std::lock_guard lock{mutex_};
    if (!unit_.empty()) {
        failure = "an instance is already running";
        return false;
    }
    std::vector<std::string> runArgs{"--user",         "--scope", "--collect",        "--quiet",
                                     "--unit=" + unit, "-p",      "TimeoutStopSec=5", "--",
                                     program};
    runArgs.insert(runArgs.end(), args.begin(), args.end());
    Argv argv{"systemd-run", runArgs};

    const pid_t pid = fork();
    if (pid < 0) {
        failure = std::string{"fork failed: "} + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        execvp("systemd-run", argv.data());
        std::_Exit(127);
    }
    child_ = pid;
    unit_ = unit;

    // systemd-run joins the scope before it execs, so the scope is up once the
    // child's cgroup names it.
    const Clock::time_point until = Clock::now() + scopeWait;
    while (!inScope(pid, unit)) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            child_ = -1;
            unit_.clear();
            failure = "could not start " + program;
            return false;
        }
        if (Clock::now() >= until) {
            lucent::error("launch", "scope {} did not appear", unit);
            endScope({"kill", "--signal=SIGKILL", unit});
            unit_.clear();
            failure = "could not create a scope for " + program;
            return false;
        }
        std::this_thread::sleep_for(step);
    }
    lucent::info("launch", "{} started in {} (pid {})", program, unit, pid);
    return true;
}

bool Instance::running() {
    const std::lock_guard lock{mutex_};
    if (unit_.empty()) {
        return false;
    }
    if (child_ >= 0) {
        int status = 0;
        const pid_t done = waitpid(child_, &status, WNOHANG);
        if (done == 0) {
            return true;
        }
        child_ = -1;
    }
    if (systemctl({"is-active", "--quiet", unit_}) == 0) {
        return true;
    }
    unit_.clear();
    return false;
}

void Instance::stop() {
    const std::lock_guard lock{mutex_};
    endScope({"stop", unit_});
}

void Instance::kill() {
    const std::lock_guard lock{mutex_};
    endScope({"kill", "--signal=SIGKILL", unit_});
}

std::string Instance::unit() const {
    const std::lock_guard lock{mutex_};
    return unit_;
}

/// Runs the systemctl verb against the scope, then reaps the started process.
/// Caller holds the mutex.
void Instance::endScope(const std::vector<std::string>& systemctlArgs) {
    if (unit_.empty()) {
        return;
    }
    // A scope that already ended on its own is not a failure to stop.
    if (systemctl(systemctlArgs) != 0 && systemctl({"is-active", "--quiet", unit_}) == 0) {
        lucent::warn("launch", "systemctl {} {} failed", systemctlArgs.front(), unit_);
    }
    reapChild();
    if (systemctlArgs.front() == "stop") {
        unit_.clear();
    }
}

/// Waits for the started process to exit, and SIGKILLs it if the scope could not.
/// Caller holds the mutex.
void Instance::reapChild() {
    if (child_ < 0) {
        return;
    }
    int status = 0;
    const Clock::time_point until = Clock::now() + reapWait;
    while (waitpid(child_, &status, WNOHANG) == 0) {
        if (Clock::now() >= until) {
            lucent::error("launch", "pid {} outlived its scope; killing it", child_);
            ::kill(child_, SIGKILL);
            while (waitpid(child_, &status, 0) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(step);
    }
    child_ = -1;
}

} // namespace iideck::launch

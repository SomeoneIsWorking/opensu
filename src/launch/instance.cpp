#include "instance.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "command.hpp"
#include "lucent/log.h"

namespace iideck::launch {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto scopeWait = std::chrono::seconds{5};
constexpr auto reapWait = std::chrono::seconds{10};
constexpr auto systemctlWait = std::chrono::seconds{30};
constexpr auto step = std::chrono::milliseconds{5};

/// Runs `systemctl --user <args>`. Returns its exit status, or -1 when it could
/// not be run.
int systemctl(const std::vector<std::string>& args) {
    std::vector<std::string> full{"--user"};
    full.insert(full.end(), args.begin(), args.end());
    return runCommand("systemctl", full, systemctlWait).value_or(-1);
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

bool Instance::start(const std::string& unit, const std::string& program,
                     const std::vector<std::string>& args, std::string& failure,
                     const std::vector<std::string>& environment) {
    const std::lock_guard lock{mutex_};
    if (!unit_.empty()) {
        failure = "an instance is already running";
        return false;
    }
    std::vector<std::string> runArgs{"--user",         "--scope", "--collect",       "--quiet",
                                     "--unit=" + unit, "-p",      "TimeoutStopSec=5"};
    for (const std::string& entry : environment) {
        runArgs.push_back("--setenv=" + entry);
    }
    runArgs.push_back("--");
    runArgs.push_back(program);
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
    exitStatus_ = -1;
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
        exitStatus_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
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

int Instance::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
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

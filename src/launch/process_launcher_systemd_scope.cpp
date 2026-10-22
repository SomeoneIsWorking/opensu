#include "process_launcher.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "child_environment.hpp"
#include "command.hpp"
#include "lucent/log.h"

namespace opensu::launch {
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

/// A wait status as a shell reports it: the exit code, or 128 plus the signal.
int statusOf(int waitStatus) {
    return WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : 128 + WTERMSIG(waitStatus);
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

/// The scope a launch runs in and the systemd-run process that joined it.
struct ProcessLauncher::Group {
    std::string unit;
    pid_t child{-1};

    /// Runs the systemctl verb against the scope, then reaps the started process.
    void end(const std::vector<std::string>& systemctlArgs, int& exitStatus);
    /// True until the started process exits; reaps it then.
    bool childRunning(int& exitStatus);
    /// Waits for the started process to exit, and SIGKILLs it if the scope could not.
    void reapChild(int& exitStatus);
};

ProcessLauncher::ProcessLauncher() : group_{std::make_unique<Group>()} {
}

ProcessLauncher::~ProcessLauncher() {
    stop();
}

bool ProcessLauncher::start(const std::string& unit, const std::string& program,
                            const std::vector<std::string>& args, std::string& failure,
                            const std::vector<std::string>& environment) {
    const std::lock_guard lock{mutex_};
    if (!group_->unit.empty()) {
        failure = "an instance is already running";
        return false;
    }
    // systemd 258 and later expand "$" in a scope's command unless told not to.
    std::vector<std::string> runArgs{
        "--user",         "--scope", "--collect",       "--quiet", "--expand-environment=no",
        "--unit=" + unit, "-p",      "TimeoutStopSec=5"};
    for (const std::string& entry : environment) {
        runArgs.push_back("--setenv=" + entry);
    }
    runArgs.push_back("--");
    runArgs.push_back(program);
    runArgs.insert(runArgs.end(), args.begin(), args.end());
    Argv argv{"systemd-run", runArgs};
    ChildEnvironment childEnvironment{environ};

    const pid_t pid = fork();
    if (pid < 0) {
        failure = std::string{"fork failed: "} + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        // The program starts with no signal blocked, whatever this thread blocks.
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        execvpe("systemd-run", argv.data(), childEnvironment.data());
        std::_Exit(127);
    }
    group_->child = pid;
    exitStatus_ = -1;
    group_->unit = unit;

    // systemd-run joins the scope before it execs, so the scope is up once the
    // child's cgroup names it.
    const Clock::time_point until = Clock::now() + scopeWait;
    while (!inScope(pid, unit)) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            group_->child = -1;
            group_->unit.clear();
            failure = "could not start " + program;
            return false;
        }
        if (Clock::now() >= until) {
            lucent::error("launch", "scope {} did not appear", unit);
            group_->end({"kill", "--signal=SIGKILL", unit}, exitStatus_);
            group_->unit.clear();
            failure = "could not create a scope for " + program;
            return false;
        }
        std::this_thread::sleep_for(step);
    }
    lucent::info("launch", "{} started in {} (pid {})", program, unit, pid);
    return true;
}

bool ProcessLauncher::running() {
    const std::lock_guard lock{mutex_};
    if (group_->unit.empty()) {
        return false;
    }
    if (group_->childRunning(exitStatus_)) {
        return true;
    }
    if (systemctl({"is-active", "--quiet", group_->unit}) == 0) {
        return true;
    }
    group_->unit.clear();
    return false;
}

bool ProcessLauncher::startedRunning() {
    const std::lock_guard lock{mutex_};
    return group_->childRunning(exitStatus_);
}

void ProcessLauncher::stop() {
    const std::lock_guard lock{mutex_};
    group_->end({"stop", group_->unit}, exitStatus_);
}

int ProcessLauncher::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
}

/// Caller holds the launcher's mutex.
void ProcessLauncher::Group::end(const std::vector<std::string>& systemctlArgs, int& exitStatus) {
    if (unit.empty()) {
        return;
    }
    lucent::info("launch", "{} {}", systemctlArgs.front(), unit);
    // A scope that already ended on its own is not a failure to stop.
    if (systemctl(systemctlArgs) != 0 && systemctl({"is-active", "--quiet", unit}) == 0) {
        lucent::warn("launch", "systemctl {} {} failed", systemctlArgs.front(), unit);
    }
    reapChild(exitStatus);
    if (systemctlArgs.front() == "stop") {
        unit.clear();
    }
}

/// Caller holds the launcher's mutex.
bool ProcessLauncher::Group::childRunning(int& exitStatus) {
    if (child < 0) {
        return false;
    }
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == 0) {
        return true;
    }
    child = -1;
    exitStatus = statusOf(status);
    return false;
}

/// Caller holds the launcher's mutex.
void ProcessLauncher::Group::reapChild(int& exitStatus) {
    if (child < 0) {
        return;
    }
    int status = 0;
    const Clock::time_point until = Clock::now() + reapWait;
    pid_t done = 0;
    while ((done = waitpid(child, &status, WNOHANG)) == 0) {
        if (Clock::now() >= until) {
            lucent::error("launch", "pid {} outlived its scope; killing it", child);
            ::kill(child, SIGKILL);
            while ((done = waitpid(child, &status, 0)) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(step);
    }
    if (done == child) {
        exitStatus = statusOf(status);
    }
    child = -1;
}

} // namespace opensu::launch

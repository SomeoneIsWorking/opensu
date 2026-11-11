#include "launch_group.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "child_environment.hpp"
#include "command.hpp"
#include "descriptor.hpp"
#include "lucent/log.h"
#include "procfs.hpp"

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

/// systemd-run's flags for a scope named `unit`. systemd 258 and later expand "$" in a scope's
/// command unless told not to.
std::vector<std::string> scopeArgs(const std::string& unit) {
    return {"--user",         "--scope", "--collect",       "--quiet", "--expand-environment=no",
            "--unit=" + unit, "-p",      "TimeoutStopSec=5"};
}

/// The scope a launch runs in and the systemd-run process that joined it.
class SystemdScope final : public LaunchGroup {
  public:
    bool start(const std::string& unit, const std::string& program,
               const std::vector<std::string>& args, std::string& failure,
               const std::vector<std::string>& environment,
               const std::filesystem::path& workingDirectory, const std::filesystem::path& output,
               int& exitStatus) override;
    bool running(int& exitStatus) override;
    bool startedRunning(int& exitStatus) override;
    void stop(int& exitStatus) override;

  private:
    /// Runs the systemctl verb against the scope, then reaps the started process.
    void end(const std::vector<std::string>& systemctlArgs, int& exitStatus);
    /// True until the started process exits; reaps it then.
    bool childRunning(int& exitStatus);
    /// Waits for the started process to exit, and SIGKILLs it if the scope could not.
    void reapChild(int& exitStatus);

    std::string unit_;
    pid_t child_{-1};
};

bool SystemdScope::start(const std::string& unit, const std::string& program,
                         const std::vector<std::string>& args, std::string& failure,
                         const std::vector<std::string>& environment,
                         const std::filesystem::path& workingDirectory,
                         const std::filesystem::path& output, int& exitStatus) {
    if (!unit_.empty()) {
        failure = "an instance is already running";
        return false;
    }
    std::vector<std::string> runArgs = scopeArgs(unit);
    for (const std::string& entry : environment) {
        runArgs.push_back("--setenv=" + entry);
    }
    if (!workingDirectory.empty()) {
        runArgs.push_back("--working-directory=" + workingDirectory.string());
    }
    runArgs.push_back("--");
    runArgs.push_back(program);
    runArgs.insert(runArgs.end(), args.begin(), args.end());
    Argv argv{"systemd-run", runArgs};
    ChildEnvironment childEnvironment;
    const Descriptor outputFile{
        output.empty() ? -1
                       : ::open(output.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644)};
    if (!output.empty() && outputFile.get() < 0) {
        failure = "cannot write " + output.string() + ": " + std::strerror(errno);
        return false;
    }

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
        if (outputFile.get() >= 0 && (dup2(outputFile.get(), STDOUT_FILENO) < 0 ||
                                      dup2(outputFile.get(), STDERR_FILENO) < 0)) {
            std::_Exit(127);
        }
        execvpe("systemd-run", argv.data(), childEnvironment.data());
        std::_Exit(127);
    }
    child_ = pid;
    exitStatus = -1;
    unit_ = unit;

    // systemd-run joins the scope before it execs, so the scope is up once the
    // child's cgroup names it.
    const Clock::time_point until = Clock::now() + scopeWait;
    while (!procfs::inUnit(pid, unit)) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            child_ = -1;
            unit_.clear();
            failure = "could not start " + programName(program);
            return false;
        }
        if (Clock::now() >= until) {
            lucent::error("launch", "scope {} did not appear", unit);
            end({"kill", "--signal=SIGKILL", unit}, exitStatus);
            unit_.clear();
            failure = "could not create a scope for " + programName(program);
            return false;
        }
        std::this_thread::sleep_for(step);
    }
    lucent::info("launch", "{} started in {} (pid {})", program, unit, pid);
    return true;
}

bool SystemdScope::running(int& exitStatus) {
    if (unit_.empty()) {
        return false;
    }
    if (childRunning(exitStatus)) {
        return true;
    }
    if (systemctl({"is-active", "--quiet", unit_}) == 0) {
        return true;
    }
    unit_.clear();
    return false;
}

bool SystemdScope::startedRunning(int& exitStatus) {
    return childRunning(exitStatus);
}

void SystemdScope::stop(int& exitStatus) {
    end({"stop", unit_}, exitStatus);
}

void SystemdScope::end(const std::vector<std::string>& systemctlArgs, int& exitStatus) {
    if (unit_.empty()) {
        return;
    }
    lucent::info("launch", "{} {}", systemctlArgs.front(), unit_);
    // A scope that already ended on its own is not a failure to stop.
    if (systemctl(systemctlArgs) != 0 && systemctl({"is-active", "--quiet", unit_}) == 0) {
        lucent::warn("launch", "systemctl {} {} failed", systemctlArgs.front(), unit_);
    }
    reapChild(exitStatus);
    if (systemctlArgs.front() == "stop") {
        unit_.clear();
    }
}

bool SystemdScope::childRunning(int& exitStatus) {
    if (child_ < 0) {
        return false;
    }
    int status = 0;
    if (waitpid(child_, &status, WNOHANG) == 0) {
        return true;
    }
    child_ = -1;
    exitStatus = statusOf(status);
    return false;
}

void SystemdScope::reapChild(int& exitStatus) {
    if (child_ < 0) {
        return;
    }
    int status = 0;
    const Clock::time_point until = Clock::now() + reapWait;
    pid_t done = 0;
    while ((done = waitpid(child_, &status, WNOHANG)) == 0) {
        if (Clock::now() >= until) {
            lucent::error("launch", "pid {} outlived its scope; killing it", child_);
            ::kill(child_, SIGKILL);
            while ((done = waitpid(child_, &status, 0)) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(step);
    }
    if (done == child_) {
        exitStatus = statusOf(status);
    }
    child_ = -1;
}

} // namespace

std::unique_ptr<LaunchGroup> makeSystemdScope() {
    return std::make_unique<SystemdScope>();
}

bool systemdScopeWorks(std::string& why) {
    std::vector<std::string> args = scopeArgs("opensu-" + std::to_string(getpid()) + "-probe.scope");
    args.emplace_back("--");
    args.emplace_back("true");
    const std::optional<Captured> ran = runCaptured("systemd-run", args, CaptureErrors::Merged);
    if (!ran) {
        why = "systemd-run could not be run";
        return false;
    }
    if (ran->status != 0) {
        std::string said = ran->output;
        while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) {
            said.pop_back();
        }
        why = "systemd-run exited with " + std::to_string(ran->status) +
              (said.empty() ? std::string{} : ": " + said);
        return false;
    }
    return true;
}

} // namespace opensu::launch

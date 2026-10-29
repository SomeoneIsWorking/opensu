#include "process_launcher.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "child_environment.hpp"
#include "command.hpp"
#include "lucent/log.h"

namespace opensu::launch {
namespace {

using Clock = std::chrono::steady_clock;

/// How long the group has to end on SIGTERM before it is killed, as the systemd scope's
/// TimeoutStopSec.
constexpr auto termWait = std::chrono::seconds{5};
constexpr auto step = std::chrono::milliseconds{5};

/// A wait status as a shell reports it: the exit code, or 128 plus the signal.
int statusOf(int waitStatus) {
    return WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : 128 + WTERMSIG(waitStatus);
}

/// posix_spawn's attributes and file actions, released on scope exit.
class SpawnSetup {
  public:
    SpawnSetup() {
        posix_spawnattr_init(&attributes_);
        posix_spawn_file_actions_init(&actions_);
    }
    ~SpawnSetup() {
        posix_spawn_file_actions_destroy(&actions_);
        posix_spawnattr_destroy(&attributes_);
    }
    SpawnSetup(const SpawnSetup&) = delete;
    SpawnSetup& operator=(const SpawnSetup&) = delete;
    SpawnSetup(SpawnSetup&&) = delete;
    SpawnSetup& operator=(SpawnSetup&&) = delete;

    posix_spawnattr_t attributes_{};
    posix_spawn_file_actions_t actions_{};
};

/// True while any process is left in the group `group`.
bool groupAlive(pid_t group) {
    return ::kill(-group, 0) == 0 || errno == EPERM;
}

} // namespace

/// The process group a launch runs in, led by the started process.
struct ProcessLauncher::Group {
    pid_t leader{-1};

    /// True until the started process exits; reaps it then.
    bool leaderRunning(int& exitStatus);
    /// SIGTERM to the group, SIGKILL to what is left after `termWait`, the leader reaped.
    void end(int& exitStatus);
};

ProcessLauncher::ProcessLauncher() : group_{std::make_unique<Group>()} {
}

bool ProcessLauncher::keepsOutput() {
    return false;
}

ProcessLauncher::~ProcessLauncher() {
    stop();
}

bool ProcessLauncher::start(const std::string& group, const std::string& program,
                            const std::vector<std::string>& args, std::string& failure,
                            const std::vector<std::string>& environment,
                            const std::filesystem::path& workingDirectory,
                            const std::filesystem::path& output) {
    const std::lock_guard lock{mutex_};
    if (group_->leader > 0) {
        failure = "an instance is already running";
        return false;
    }
    Argv argv{program, args};
    ChildEnvironment childEnvironment{environment};
    SpawnSetup setup;
    // A group of its own, led by the program, with no signal blocked or ignored.
    sigset_t none;
    sigemptyset(&none);
    sigset_t all;
    sigfillset(&all);
    sigdelset(&all, SIGKILL);
    sigdelset(&all, SIGSTOP);
    constexpr int flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF |
                          POSIX_SPAWN_CLOEXEC_DEFAULT;
    posix_spawnattr_setflags(&setup.attributes_, static_cast<short>(flags));
    posix_spawnattr_setpgroup(&setup.attributes_, 0);
    posix_spawnattr_setsigmask(&setup.attributes_, &none);
    posix_spawnattr_setsigdefault(&setup.attributes_, &all);
    // Every other descriptor of openSU's stays out of the game.
    posix_spawn_file_actions_addinherit_np(&setup.actions_, STDIN_FILENO);
    if (output.empty()) {
        posix_spawn_file_actions_addinherit_np(&setup.actions_, STDOUT_FILENO);
        posix_spawn_file_actions_addinherit_np(&setup.actions_, STDERR_FILENO);
    } else {
        posix_spawn_file_actions_addopen(&setup.actions_, STDOUT_FILENO, output.c_str(),
                                         O_WRONLY | O_CREAT | O_APPEND, 0644);
        posix_spawn_file_actions_adddup2(&setup.actions_, STDOUT_FILENO, STDERR_FILENO);
    }
    if (!workingDirectory.empty()) {
        posix_spawn_file_actions_addchdir_np(&setup.actions_, workingDirectory.c_str());
    }
    pid_t pid = -1;
    const int spawned = posix_spawnp(&pid, program.c_str(), &setup.actions_, &setup.attributes_,
                                     argv.data(), childEnvironment.data());
    if (spawned != 0) {
        failure = "could not start " + programName(program) + ": " + std::strerror(spawned);
        return false;
    }
    group_->leader = pid;
    exitStatus_ = -1;
    lucent::info("launch", "{} started in its process group {} for {}", program, pid, group);
    return true;
}

bool ProcessLauncher::running() {
    const std::lock_guard lock{mutex_};
    if (group_->leader <= 0) {
        return false;
    }
    if (group_->leaderRunning(exitStatus_) || groupAlive(group_->leader)) {
        return true;
    }
    group_->leader = -1;
    return false;
}

bool ProcessLauncher::startedRunning() {
    const std::lock_guard lock{mutex_};
    return group_->leaderRunning(exitStatus_);
}

void ProcessLauncher::stop() {
    const std::lock_guard lock{mutex_};
    group_->end(exitStatus_);
}

int ProcessLauncher::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
}

/// Caller holds the launcher's mutex. The group keeps the leader's pid as its id after the leader
/// is reaped.
bool ProcessLauncher::Group::leaderRunning(int& exitStatus) {
    if (leader <= 0) {
        return false;
    }
    int status = 0;
    const pid_t done = waitpid(leader, &status, WNOHANG);
    if (done == 0) {
        return true;
    }
    if (done == leader) {
        exitStatus = statusOf(status);
    }
    return false;
}

/// Caller holds the launcher's mutex.
void ProcessLauncher::Group::end(int& exitStatus) {
    if (leader <= 0) {
        return;
    }
    lucent::info("launch", "stopping process group {}", leader);
    ::kill(-leader, SIGTERM);
    const Clock::time_point until = Clock::now() + termWait;
    while (Clock::now() < until) {
        static_cast<void>(leaderRunning(exitStatus));
        if (!groupAlive(leader)) {
            break;
        }
        std::this_thread::sleep_for(step);
    }
    if (groupAlive(leader)) {
        lucent::warn("launch", "process group {} outlived SIGTERM; killing it", leader);
        ::kill(-leader, SIGKILL);
    }
    int status = 0;
    pid_t done = 0;
    while ((done = waitpid(leader, &status, 0)) < 0 && errno == EINTR) {
    }
    if (done == leader) {
        exitStatus = statusOf(status);
    }
    leader = -1;
}

} // namespace opensu::launch

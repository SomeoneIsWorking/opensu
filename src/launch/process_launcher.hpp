// process_launcher — starts a program in a group of its own that owns everything it starts, and
// ends the whole group. One backend per OS, picked in CMakeLists.txt: a transient systemd user
// scope on Linux (`process_launcher_systemd_scope.cpp`); elsewhere a launch refuses until that
// OS's backend lands (`process_launcher_unavailable.cpp`).
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace opensu::launch {

/// Runs a command inside its own group. On Linux the group is a systemd scope, whose cgroup is
/// the ownership: processes that setsid or double-fork stay in it, so stopping the scope ends all
/// of them. Every call is thread-safe, so a controller handler can stop a launch another thread is
/// waiting on.
class ProcessLauncher {
  public:
    ProcessLauncher();
    ProcessLauncher(const ProcessLauncher&) = delete;
    ProcessLauncher& operator=(const ProcessLauncher&) = delete;
    ~ProcessLauncher();

    /// Starts `program` in the group `group` (the scope's unit name on Linux), with each
    /// "NAME=value" of `environment` added to its environment. Returns once the group exists, so
    /// a following stop() always reaches it. The program starts with no signal blocked. False with
    /// `failure` when it cannot be started, which on an OS without a backend is every time.
    [[nodiscard]] bool start(const std::string& group, const std::string& program,
                             const std::vector<std::string>& args, std::string& failure,
                             const std::vector<std::string>& environment = {});

    /// True while anything is left in the group. The started process may exit while processes it
    /// spawned carry on, which still counts as running.
    [[nodiscard]] bool running();

    /// Asks every process in the group to end and returns once the group is gone: on Linux
    /// SIGTERM, and systemd SIGKILLs whatever is left after TimeoutStopSec (5 s). Then reaps the
    /// started one.
    void stop();

    /// How the started process ended (128 plus the signal when a signal did it), or -1 while it
    /// has not been seen to exit. Observed by running() and stop().
    [[nodiscard]] int exitStatus() const;

  private:
    /// What the OS's backend keeps of the running group.
    struct Group;

    mutable std::mutex mutex_;
    std::unique_ptr<Group> group_;
    int exitStatus_{-1};
};

} // namespace opensu::launch

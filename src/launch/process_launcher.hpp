// process_launcher — starts a program in a group of its own that owns everything it starts, and
// ends the whole group. One backend per OS, picked in CMakeLists.txt: a transient systemd user
// scope on Linux (`process_launcher_systemd_scope.cpp`), a Job Object on Windows
// (`process_launcher_job_object.cpp`); on Android an intent from the shell's Activity, which owns
// only openSU's own runner (`process_launcher_intent.cpp`); elsewhere a launch refuses until that
// OS's backend lands (`process_launcher_unavailable.cpp`).
#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace opensu::launch {

/// Runs a command inside its own group. On Linux the group is a systemd scope, whose cgroup is
/// the ownership: processes that setsid or double-fork stay in it, so stopping the scope ends all
/// of them. On Windows it is a Job Object (kill on close), which every child process joins.
/// Every call is thread-safe, so a controller handler can stop a launch another thread is
/// waiting on.
class ProcessLauncher {
  public:
    ProcessLauncher();
    ProcessLauncher(const ProcessLauncher&) = delete;
    /// Whether `program` names what this backend starts: an executable file, by path or on
    /// `executablePath`; on Android also another app's activity, which start() checks.
    [[nodiscard]] static bool startable(const std::string& program,
                                        const std::vector<std::filesystem::path>& executablePath);
    /// Whether a group started with no `output` file still has its output kept: on Linux it is
    /// openSU's own stdout, which the session keeps; on Android the started app logs to logcat
    /// itself. A program in a Job Object gets the stdout of openSU, a GUI program, which has none.
    [[nodiscard]] static bool keepsOutput();
    ProcessLauncher& operator=(const ProcessLauncher&) = delete;
    ~ProcessLauncher();

    /// Starts `program` in the group `group` (the scope's unit name on Linux), with each
    /// "NAME=value" of `environment` added to its environment, in `workingDirectory` (openSU's
    /// own when empty), its stdout and stderr appended to `output` when one is given (openSU's
    /// own otherwise). Returns once the group exists, so a following stop() always reaches it.
    /// The program starts with no signal blocked. False with `failure` when it cannot be started,
    /// which on an OS without a backend is every time.
    [[nodiscard]] bool start(const std::string& group, const std::string& program,
                             const std::vector<std::string>& args, std::string& failure,
                             const std::vector<std::string>& environment = {},
                             const std::filesystem::path& workingDirectory = {},
                             const std::filesystem::path& output = {});

    /// True while anything is left in the group. The started process may exit while processes it
    /// spawned carry on, which still counts as running.
    [[nodiscard]] bool running();

    /// True until the started process itself exits. It keeps its pid through every exec, so this
    /// holds while its command line is being replaced.
    [[nodiscard]] bool startedRunning();

    /// Asks every process in the group to end and returns once the group is gone: on Linux
    /// SIGTERM, and systemd SIGKILLs whatever is left after TimeoutStopSec (5 s); on Windows
    /// WM_CLOSE to the group's windows, and the job is terminated after 5 s. Then reaps the
    /// started one.
    void stop();

    /// How the started process ended (128 plus the signal when a signal did it; its exit code on
    /// Windows), or -1 while it has not been seen to exit. Observed by running() and stop().
    [[nodiscard]] int exitStatus() const;

  private:
    /// What the OS's backend keeps of the running group.
    struct Group;

    mutable std::mutex mutex_;
    std::unique_ptr<Group> group_;
    int exitStatus_{-1};
};

} // namespace opensu::launch

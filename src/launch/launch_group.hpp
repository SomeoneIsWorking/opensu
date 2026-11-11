// launch_group — the Linux ProcessLauncher's backends, one per `Containment`, behind the one
// interface `process_launcher_linux.cpp` dispatches to.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "containment.hpp"

namespace opensu::launch {

/// One started program's group. The caller holds the ProcessLauncher's mutex for every call and
/// owns `exitStatus`, which each call updates as ProcessLauncher documents.
class LaunchGroup {
  public:
    LaunchGroup() = default;
    LaunchGroup(const LaunchGroup&) = delete;
    LaunchGroup& operator=(const LaunchGroup&) = delete;
    LaunchGroup(LaunchGroup&&) = delete;
    LaunchGroup& operator=(LaunchGroup&&) = delete;
    virtual ~LaunchGroup() = default;

    [[nodiscard]] virtual bool start(const std::string& group, const std::string& program,
                                     const std::vector<std::string>& args, std::string& failure,
                                     const std::vector<std::string>& environment,
                                     const std::filesystem::path& workingDirectory,
                                     const std::filesystem::path& output, int& exitStatus) = 0;
    [[nodiscard]] virtual bool running(int& exitStatus) = 0;
    [[nodiscard]] virtual bool startedRunning(int& exitStatus) = 0;
    virtual void stop(int& exitStatus) = 0;
};

/// A wait status as a shell reports it: the exit code, or 128 plus the signal.
[[nodiscard]] int exitStatusOf(int waitStatus);

/// The backend for `containment`; throws std::invalid_argument for one Linux has none of.
[[nodiscard]] std::unique_ptr<LaunchGroup> makeLaunchGroup(Containment containment);

/// A transient systemd user scope per launch (`process_launcher_systemd_scope.cpp`).
[[nodiscard]] std::unique_ptr<LaunchGroup> makeSystemdScope();

/// A keeper process per launch (`process_launcher_keeper.cpp`).
[[nodiscard]] std::unique_ptr<LaunchGroup> makeKeeper();

/// Whether systemd-run starts a program in a user scope here, tried once with the flags every
/// launch uses; false with what systemd-run said in `why`.
[[nodiscard]] bool systemdScopeWorks(std::string& why);

} // namespace opensu::launch

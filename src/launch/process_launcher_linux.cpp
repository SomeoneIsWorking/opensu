#include "process_launcher.hpp"

#include <memory>
#include <string>
#include <vector>

#include "launch_group.hpp"

namespace opensu::launch {

struct ProcessLauncher::Group {
    std::unique_ptr<LaunchGroup> held;
};

ProcessLauncher::ProcessLauncher(Containment containment)
    : group_{std::make_unique<Group>(Group{makeLaunchGroup(containment)})} {
}

bool ProcessLauncher::keepsOutput() {
    return true;
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
    return group_->held->start(group, program, args, failure, environment, workingDirectory,
                               output, exitStatus_);
}

bool ProcessLauncher::running() {
    const std::lock_guard lock{mutex_};
    return group_->held->running(exitStatus_);
}

bool ProcessLauncher::startedRunning() {
    const std::lock_guard lock{mutex_};
    return group_->held->startedRunning(exitStatus_);
}

void ProcessLauncher::stop() {
    const std::lock_guard lock{mutex_};
    group_->held->stop(exitStatus_);
}

int ProcessLauncher::exitStatus() const {
    const std::lock_guard lock{mutex_};
    return exitStatus_;
}

} // namespace opensu::launch

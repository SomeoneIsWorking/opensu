#include "process_launcher.hpp"

namespace opensu::launch {

/// No group is ever started here.
struct ProcessLauncher::Group {};

ProcessLauncher::ProcessLauncher() = default;

ProcessLauncher::~ProcessLauncher() = default;

bool ProcessLauncher::start(const std::string& /*group*/, const std::string& program,
                            const std::vector<std::string>& /*args*/, std::string& failure,
                            const std::vector<std::string>& /*environment*/) {
    failure = program + " cannot be started: launching is not available on " OPENSU_SYSTEM_NAME;
    return false;
}

bool ProcessLauncher::running() {
    return false;
}

void ProcessLauncher::stop() {
}

int ProcessLauncher::exitStatus() const {
    return exitStatus_;
}

} // namespace opensu::launch

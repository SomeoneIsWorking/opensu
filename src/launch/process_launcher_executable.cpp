#include "command.hpp"
#include "process_launcher.hpp"

namespace opensu::launch {

bool ProcessLauncher::startable(const std::string& program,
                                const std::vector<std::filesystem::path>& executablePath) {
    return !resolveExecutable(program, executablePath).empty();
}

} // namespace opensu::launch

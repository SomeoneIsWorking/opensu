#include "shell_session.hpp"

namespace opensu::app {

std::unique_ptr<ShellSession> openShellSession(const config::Config& /*config*/) {
    return nullptr;
}

std::optional<int> runInOwnSession(const config::Config& /*config*/,
                                   const config::Arguments& /*arguments*/,
                                   const std::vector<std::string>& /*args*/) {
    return std::nullopt;
}

} // namespace opensu::app

#include "fumarole_install_job.hpp"

#include "library/fumarole_installs.hpp"

namespace opensu::app {

FumaroleInstallJob::FumaroleInstallJob(std::string program)
    : CliInstallJob{std::move(program), "fumarole", "fumarole"} {
}

FumaroleInstallJob::~FumaroleInstallJob() {
    halt();
}

std::vector<std::string> FumaroleInstallJob::arguments(const std::stop_token& /*stop*/,
                                                       const std::string& appId) {
    std::vector<std::string> arguments{"install", appId, "--os", "windows", "--json"};
    if (folder_) {
        arguments.insert(arguments.end(), {"--path", folder_->string()});
    }
    return arguments;
}

std::optional<double> FumaroleInstallJob::progressIn(std::string_view line) const {
    return library::fumarole::installProgress(line);
}

std::optional<std::string> FumaroleInstallJob::failureIn(std::string_view line) const {
    return library::fumarole::installFailure(line);
}

} // namespace opensu::app

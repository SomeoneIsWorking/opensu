#include "epic_install_job.hpp"

#include <utility>

#include "library/epic.hpp"
#include "library/install_log.hpp"

namespace opensu::app {

EpicInstallJob::EpicInstallJob() : CliInstallJob{"legendary", "legendary", "epic"} {
}

EpicInstallJob::EpicInstallJob(std::string binary)
    : CliInstallJob{std::move(binary), "legendary", "epic"} {
}

EpicInstallJob::~EpicInstallJob() {
    halt();
}

std::vector<std::string> EpicInstallJob::arguments(const std::string& appName) {
    // -y answers legendary's prompts; --skip-sdl keeps it from asking which optional packs to
    // fetch, as its stdin is closed.
    std::vector<std::string> arguments{"install", appName, "-y", "--skip-sdl"};
    if (folder_) {
        arguments.insert(arguments.end(), {"--base-path", folder_->string()});
    }
    return arguments;
}

std::optional<double> EpicInstallJob::progressIn(std::string_view line) const {
    return library::install_log::progress(line);
}

std::optional<std::string> EpicInstallJob::failureIn(std::string_view line) const {
    return library::epic::installFailure(line);
}

} // namespace opensu::app

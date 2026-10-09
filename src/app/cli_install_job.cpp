#include "cli_install_job.hpp"

#include <stdexcept>
#include <utility>

#include "launch/command.hpp"
#include "launch/launch_progress.hpp"
#include "lucent/log.h"

namespace opensu::app {

CliInstallJob::CliInstallJob(std::string program, std::string tool, std::string logTag)
    : program_{std::move(program)}, tool_{std::move(tool)}, logTag_{std::move(logTag)} {
}

std::optional<std::string> CliInstallJob::ended(const std::string&, bool) {
    return std::nullopt;
}

void CliInstallJob::run(const std::stop_token& stop, const std::string& appId) {
    post(Report{.line = "Starting the download"});
    std::vector<std::string> args;
    try {
        args = arguments(appId);
    } catch (const std::runtime_error& error) {
        post(Report{.finished = true, .failure = error.what()});
        return;
    }
    std::optional<std::string> failure;
    const std::optional<int> status = launch::runStreaming(
        program_, args,
        [this, &failure](std::string_view line) {
            if (const std::optional<double> progress = progressIn(line)) {
                post(Report{
                    .line = launch::describe(launch::LaunchProgress{
                        .stage = launch::LaunchProgress::Stage::Installing, .fraction = *progress}),
                    .fraction = progress});
            } else if (std::optional<std::string> reason = failureIn(line); reason && !failure) {
                failure = std::move(reason);
            }
        },
        stop);
    const bool installed = status && *status == 0;
    std::optional<std::string> recordFailure;
    try {
        recordFailure = ended(appId, installed);
    } catch (const std::runtime_error& error) {
        recordFailure = error.what();
    }
    if (stop.stop_requested()) {
        return;
    }
    if (!status) {
        post(Report{.finished = true, .failure = tool_ + " is not installed"});
    } else if (*status != 0) {
        post(Report{.finished = true,
                    .failure = failure.value_or(tool_ + " stopped with status " +
                                                std::to_string(*status))});
    } else if (recordFailure) {
        post(Report{.finished = true, .failure = *recordFailure});
    } else {
        lucent::info(logTag_, "{} is installed", title());
        post(Report{.finished = true});
    }
}

} // namespace opensu::app

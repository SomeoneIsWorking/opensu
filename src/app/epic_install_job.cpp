#include "epic_install_job.hpp"

#include <optional>
#include <string_view>
#include <utility>

#include "launch/command.hpp"
#include "launch/launch_progress.hpp"
#include "library/epic.hpp"
#include "lucent/log.h"

namespace iideck::app {

EpicInstallJob::EpicInstallJob() : binary_{"legendary"} {
}

EpicInstallJob::EpicInstallJob(std::string binary) : binary_{std::move(binary)} {
}

EpicInstallJob::~EpicInstallJob() {
    halt();
}

void EpicInstallJob::run(const std::stop_token& stop, const std::string& appName) {
    post(Report{.line = "Starting the download"});
    std::optional<std::string> failure;
    // -y answers legendary's prompts; --skip-sdl keeps it from asking which optional packs to
    // fetch, as its stdin is closed.
    const std::optional<int> status = launch::runStreaming(
        binary_, {"install", appName, "-y", "--skip-sdl"},
        [this, &failure](std::string_view line) {
            if (const std::optional<double> progress = library::epic::installProgress(line)) {
                post(Report{
                    .line = launch::describe(launch::LaunchProgress{
                        .stage = launch::LaunchProgress::Stage::Installing,
                        .fraction = *progress}),
                    .fraction = progress});
            } else if (std::optional<std::string> reason = library::epic::installFailure(line);
                       reason && !failure) {
                failure = std::move(reason);
            }
        },
        stop);
    if (stop.stop_requested()) {
        return;
    }
    if (!status) {
        post(Report{.finished = true, .failure = "legendary is not installed"});
    } else if (*status != 0) {
        post(Report{.finished = true,
                    .failure = failure.value_or("legendary stopped with status " +
                                                std::to_string(*status))});
    } else {
        lucent::info("epic", "{} is installed", title());
        post(Report{.finished = true});
    }
}

} // namespace iideck::app

#include "install_job.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

#include "launch/launch_progress.hpp"
#include "lucent/log.h"

namespace iideck::app {
namespace {

constexpr auto pollInterval = std::chrono::seconds{1};

} // namespace

InstallJob::InstallJob(steam::Client& steam) : steam_{steam} {
}

bool InstallJob::start(std::string appId, std::string title) {
    {
        const std::lock_guard lock{mutex_};
        if (running_) {
            return false;
        }
        running_ = true;
        title_ = std::move(title);
        pending_.reset();
    }
    thread_ = std::jthread{[this, appId = std::move(appId)](const std::stop_token& stop) {
        run(stop, appId);
    }};
    return true;
}

bool InstallJob::running() const {
    const std::lock_guard lock{mutex_};
    return running_;
}

std::string InstallJob::title() const {
    const std::lock_guard lock{mutex_};
    return title_;
}

std::optional<InstallJob::Report> InstallJob::take() {
    const std::lock_guard lock{mutex_};
    std::optional<Report> report = std::exchange(pending_, std::nullopt);
    if (report && report->finished) {
        running_ = false;
    }
    return report;
}

void InstallJob::post(Report report) {
    const std::lock_guard lock{mutex_};
    // A finished report is never overwritten by an older line.
    if (!pending_ || !pending_->finished) {
        pending_ = std::move(report);
    }
}

void InstallJob::run(const std::stop_token& stop, const std::string& appId) {
    const std::string name = title();
    post(Report{.line = "Starting Steam"});
    try {
        steam_.install(appId, name);
    } catch (const std::runtime_error& error) {
        lucent::error("steam", "cannot install {}: {}", name, error.what());
        post(Report{.finished = true, .failure = error.what()});
        return;
    }
    while (!stop.stop_requested()) {
        const launch::SteamState state = steam_.state();
        if (state == launch::SteamState::Failed || state == launch::SteamState::Blocked) {
            post(Report{.finished = true, .failure = "Steam is not running"});
            return;
        }
        if (state == launch::SteamState::Initializing) {
            post(Report{.line = launch::describe(launch::LaunchProgress{
                            .stage = launch::LaunchProgress::Stage::WaitingForSteam})});
        } else if (const std::optional<double> progress = steam_.updateProgress(appId)) {
            post(Report{
                .line = launch::describe(launch::LaunchProgress{
                    .stage = launch::LaunchProgress::Stage::Installing, .fraction = *progress}),
                .fraction = progress});
        } else if (steam_.installed(appId)) {
            lucent::info("steam", "{} is installed", name);
            post(Report{.finished = true});
            return;
        } else {
            post(Report{.finished = true, .failure = "Steam did not install it"});
            return;
        }
        std::this_thread::sleep_for(pollInterval);
    }
}

} // namespace iideck::app

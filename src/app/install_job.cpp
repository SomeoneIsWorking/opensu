#include "install_job.hpp"

#include <chrono>
#include <utility>

#include "launch/launch_progress.hpp"
#include "lucent/log.h"

namespace iideck::app {
namespace {

constexpr auto pollInterval = std::chrono::seconds{1};
constexpr auto wizardPoll = std::chrono::milliseconds{250};
/// How long Steam's installer may take to queue the download without asking anything.
constexpr auto wizardLimit = std::chrono::seconds{60};
/// How long a queued download may take to show in Steam's queue.
constexpr auto queueLimit = std::chrono::seconds{30};
constexpr auto readyLimit = std::chrono::minutes{2};

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
        decision_.reset();
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

void InstallJob::decide(bool accepted) {
    {
        const std::lock_guard lock{mutex_};
        decision_ = accepted;
    }
    decided_.notify_all();
}

std::optional<bool> InstallJob::awaitDecision(const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    if (!decided_.wait(lock, stop, [this] {
            return decision_.has_value();
        })) {
        return std::nullopt;
    }
    return std::exchange(decision_, std::nullopt);
}

void InstallJob::post(Report report) {
    const std::lock_guard lock{mutex_};
    // A finished report is never overwritten by an older line.
    if (!pending_ || !pending_->finished) {
        pending_ = std::move(report);
    }
}

void InstallJob::run(const std::stop_token& stop, const std::string& appId) {
    post(Report{.line = launch::describe(launch::LaunchProgress{
                    .stage = launch::LaunchProgress::Stage::WaitingForSteam})});
    const launch::SteamState state = steam_.waitReady(readyLimit, [&stop] {
        return stop.stop_requested();
    });
    if (state != launch::SteamState::Ready) {
        post(Report{.finished = true, .failure = "Steam is not running"});
        return;
    }
    if (queue(stop, appId)) {
        follow(stop, appId);
    }
}

bool InstallJob::queue(const std::stop_token& stop, const std::string& appId) {
    steam::InstallWizard& installer = steam_.installer();
    steam::InstallStep step = installer.open(appId);
    auto deadline = std::chrono::steady_clock::now() + wizardLimit;
    while (!stop.stop_requested()) {
        switch (step.kind) {
        case steam::InstallStep::Kind::Queued:
            return true;
        case steam::InstallStep::Kind::Failed:
            post(Report{.finished = true, .failure = step.failure});
            return false;
        case steam::InstallStep::Kind::NeedsEula: {
            post(Report{.line = "Accept the licence agreement to install", .eulas = step.eulas});
            const std::optional<bool> accepted = awaitDecision(stop);
            if (!accepted) {
                installer.cancel();
                return false;
            }
            if (!*accepted) {
                installer.cancel();
                post(Report{.finished = true, .failure = "the licence agreement was declined"});
                return false;
            }
            post(Report{.line = "Starting the download"});
            step = installer.accept(step.eulas);
            deadline = std::chrono::steady_clock::now() + wizardLimit;
            continue;
        }
        case steam::InstallStep::Kind::Working:
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            installer.cancel();
            post(Report{.finished = true, .failure = "Steam's installer did not finish"});
            return false;
        }
        std::this_thread::sleep_for(wizardPoll);
        step = installer.poll();
    }
    installer.cancel();
    return false;
}

void InstallJob::follow(const std::stop_token& stop, const std::string& appId) {
    const std::string name = title();
    // Steam lists the download a moment after queueing it, and marks the app installed a
    // moment after finishing it.
    auto lastSeen = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        if (steam_.state() != launch::SteamState::Ready) {
            post(Report{.finished = true, .failure = "Steam is not running"});
            return;
        }
        if (const std::optional<double> progress = steam_.updateProgress(appId)) {
            lastSeen = std::chrono::steady_clock::now();
            post(Report{
                .line = launch::describe(launch::LaunchProgress{
                    .stage = launch::LaunchProgress::Stage::Installing, .fraction = *progress}),
                .fraction = progress});
        } else if (steam_.installed(appId)) {
            lucent::info("steam", "{} is installed", name);
            post(Report{.finished = true});
            return;
        } else if (std::chrono::steady_clock::now() - lastSeen > queueLimit) {
            post(Report{.finished = true, .failure = "Steam stopped downloading it"});
            return;
        }
        std::this_thread::sleep_for(pollInterval);
    }
}

} // namespace iideck::app

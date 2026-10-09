#include "install_job.hpp"

#include <utility>

namespace opensu::app {

void InstallJob::halt() {
    thread_.request_stop();
    if (thread_.joinable()) {
        thread_.join();
    }
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

} // namespace opensu::app

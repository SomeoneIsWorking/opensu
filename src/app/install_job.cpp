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
        over_ = false;
        title_ = std::move(title);
        pending_.reset();
        decision_.reset();
        status_ = Status{.serial = status_ ? status_->serial + 1 : 1,
                         .appId = appId,
                         .title = title_,
                         .phase = Phase::Running};
    }
    thread_ = std::jthread{[this, appId = std::move(appId)](const std::stop_token& stop) {
        run(stop, appId);
        if (stop.stop_requested()) {
            post(Report{.line = "Cancelled", .finished = true, .cancelled = true});
        }
    }};
    return true;
}

std::optional<InstallJob::Status> InstallJob::status() const {
    const std::lock_guard lock{mutex_};
    return status_;
}

void InstallJob::cancel() {
    {
        const std::lock_guard lock{mutex_};
        if (!running_ || over_) {
            return;
        }
    }
    thread_.request_stop();
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
    // The first finished report is the job's end; nothing after it counts.
    if (over_) {
        return;
    }
    over_ = report.finished;
    if (status_) {
        status_->line = report.line;
        if (report.fraction) {
            status_->fraction = report.fraction;
        }
        status_->failure = report.failure;
        if (report.cancelled) {
            status_->phase = Phase::Cancelled;
        } else if (report.finished) {
            status_->phase = report.failure.empty() ? Phase::Done : Phase::Failed;
        } else {
            status_->phase = report.licence ? Phase::Licence : Phase::Running;
        }
    }
    pending_ = std::move(report);
}

} // namespace opensu::app

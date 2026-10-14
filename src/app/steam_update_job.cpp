#include "steam_update_job.hpp"

#include "lucent/log.h"

namespace opensu::app {

SteamUpdateJob::~SteamUpdateJob() {
    halt();
}

void SteamUpdateJob::run(const std::stop_token& stop, const std::string& appId) {
    if (!ready_()) {
        post(Report{.finished = true, .failure = "Steam is not running"});
        return;
    }
    post(Report{.line = "Asking Steam"});
    std::string refused = control_.moveToTop(appId);
    if (stop.stop_requested()) {
        return;
    }
    if (!refused.empty()) {
        post(Report{.finished = true, .failure = std::move(refused)});
        return;
    }
    lucent::info("steam", "{}: update queued", title());
    post(Report{.line = "update queued in Steam", .finished = true});
}

} // namespace opensu::app

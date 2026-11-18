#include "close_asks.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"

namespace opensu::app {

void CloseAsks::playing(std::string id, std::string title, std::vector<std::string> seats) {
    const std::lock_guard lock{mutex_};
    runningId_ = std::move(id);
    runningTitle_ = std::move(title);
    seats_ = std::move(seats);
}

lan::wire::PlayVerdict CloseAsks::ask(const lan::Peer& peer, const std::string& gameId,
                                      const std::string& profile, bool linked) {
    const std::lock_guard lock{mutex_};
    const auto waiting = waiting_.find(peer.id);
    if (runningId_ != gameId) {
        if (waiting != waiting_.end()) {
            waiting_.erase(waiting);
        }
        return lan::wire::PlayVerdict{.verdict = "closed", .port = 0, .why = {}};
    }
    if (waiting != waiting_.end() && !waiting->second.empty()) {
        std::string why = std::move(waiting->second);
        waiting_.erase(waiting);
        return lan::wire::PlayVerdict{.verdict = "declined", .port = 0, .why = std::move(why)};
    }
    if (waiting == waiting_.end()) {
        if (!linked || std::ranges::find(seats_, profile) == seats_.end()) {
            return lan::wire::PlayVerdict{.verdict = "declined",
                                          .port = 0,
                                          .why = runningTitle_ +
                                                 " is another profile's game; ask to join it"};
        }
        waiting_.emplace(peer.id, std::string{});
        closing_ = true;
        lucent::info("remote", "{} closes {} here", peer.name, runningTitle_);
    }
    return lan::wire::PlayVerdict{.verdict = "waiting", .port = 0, .why = {}};
}

void CloseAsks::service() {
    std::unique_lock lock{mutex_};
    if (!closing_) {
        return;
    }
    closing_ = false;
    if (runningId_.empty()) {
        return;
    }
    const std::string title = runningTitle_;
    lock.unlock();
    std::string refused = close_();
    lock.lock();
    if (refused.empty()) {
        lucent::info("remote", "closed {} for another openSU", title);
        return;
    }
    lucent::warn("remote", "{} did not close for another openSU: {}", title, refused);
    for (auto& [peer, why] : waiting_) {
        why = refused;
    }
}

} // namespace opensu::app

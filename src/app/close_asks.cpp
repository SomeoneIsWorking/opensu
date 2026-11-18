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
    if (runningId_ != gameId) {
        std::erase_if(asks_, [&peer](const Asked& asked) {
            return asked.peer.id == peer.id;
        });
        return lan::wire::PlayVerdict{.verdict = "closed", .port = 0, .why = {}};
    }
    const auto found = std::ranges::find(asks_, peer.id, [](const Asked& asked) {
        return asked.peer.id;
    });
    if (found != asks_.end() && found->gameId == gameId) {
        if (found->verdict == Asked::Verdict::Declined) {
            std::string why = found->why;
            asks_.erase(found);
            return lan::wire::PlayVerdict{.verdict = "declined", .port = 0, .why = std::move(why)};
        }
        return lan::wire::PlayVerdict{.verdict = "waiting", .port = 0, .why = {}};
    }
    if (found != asks_.end()) {
        asks_.erase(found);
    }
    const bool seated = linked && std::ranges::find(seats_, profile) != seats_.end();
    asks_.push_back(Asked{.peer = peer,
                          .gameId = gameId,
                          .verdict = seated ? Asked::Verdict::Allowed : Asked::Verdict::Waiting,
                          .why = {},
                          .at = Clock::now()});
    closing_ = closing_ || seated;
    lucent::info("remote", "{} asks to close {}{}", peer.name, runningTitle_,
                 seated ? ", playing it as a linked profile" : "; the player here is asked");
    return lan::wire::PlayVerdict{.verdict = "waiting", .port = 0, .why = {}};
}

std::optional<CloseAsks::Question> CloseAsks::question() const {
    const std::lock_guard lock{mutex_};
    const auto waiting = std::ranges::find(asks_, Asked::Verdict::Waiting, &Asked::verdict);
    if (waiting == asks_.end() || closing_) {
        return std::nullopt;
    }
    return Question{.peer = waiting->peer, .title = runningTitle_};
}

void CloseAsks::answer(const std::string& peerId, bool allow) {
    const std::lock_guard lock{mutex_};
    for (Asked& asked : asks_) {
        if (asked.peer.id == peerId && asked.verdict == Asked::Verdict::Waiting) {
            asked.verdict = allow ? Asked::Verdict::Allowed : Asked::Verdict::Declined;
            asked.why = allow ? std::string{} : "its player keeps playing";
            asked.at = Clock::now();
            closing_ = closing_ || allow;
        }
    }
}

void CloseAsks::service() {
    std::unique_lock lock{mutex_};
    const Clock::time_point now = Clock::now();
    for (Asked& asked : asks_) {
        if (asked.verdict == Asked::Verdict::Waiting && now - asked.at > patience) {
            asked.verdict = Asked::Verdict::Declined;
            asked.why = "its player did not answer";
            asked.at = now;
        }
    }
    std::erase_if(asks_, [now](const Asked& asked) {
        return asked.verdict != Asked::Verdict::Waiting && now - asked.at > answerKept;
    });
    if (!closing_) {
        return;
    }
    closing_ = false;
    if (runningId_.empty()) {
        return;
    }
    const std::string title = runningTitle_;
    lock.unlock();
    const std::string refused = close_();
    lock.lock();
    if (refused.empty()) {
        lucent::info("remote", "closed {} for another openSU", title);
        return;
    }
    lucent::warn("remote", "{} did not close for another openSU: {}", title, refused);
    for (Asked& asked : asks_) {
        if (asked.verdict == Asked::Verdict::Allowed) {
            asked.verdict = Asked::Verdict::Declined;
            asked.why = refused;
            asked.at = Clock::now();
        }
    }
}

} // namespace opensu::app

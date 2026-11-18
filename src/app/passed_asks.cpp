#include "passed_asks.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"

#include "remote_play.hpp"

namespace opensu::app {

std::optional<std::string> PassedAsks::passed(const lan::Peer& host,
                                              const lan::wire::JoinAsk& ask) {
    const std::lock_guard lock{mutex_};
    if (streamingFrom_ != host.id) {
        return "this openSU does not play that game from there now";
    }
    std::erase_if(asks_, [&ask](const Passed& passed) {
        return passed.asker.id == ask.asker;
    });
    asks_.push_back(Passed{.host = host,
                           .asker = lan::Peer{.id = ask.asker, .name = ask.name},
                           .title = ask.title,
                           .at = std::chrono::steady_clock::now()});
    lucent::info("remote", "{} passes {}'s ask to join {}", host.name, ask.name, ask.title);
    return std::nullopt;
}

void PassedAsks::service(const std::string& streamingFrom) {
    {
        const std::lock_guard lock{*newsMutex_};
        for (const std::string& text : *news_) {
            hooks_.say(text, true);
        }
        news_->clear();
    }
    const std::lock_guard lock{mutex_};
    streamingFrom_ = streamingFrom;
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(asks_, [&](const Passed& passed) {
        return passed.host.id != streamingFrom_ || now - passed.at > RemotePlay::askPatience;
    });
}

std::optional<PassedAsks::Passed> PassedAsks::next() const {
    const std::lock_guard lock{mutex_};
    return asks_.empty() ? std::nullopt : std::optional{asks_.front()};
}

void PassedAsks::answer(const std::string& askerId, bool allow) {
    std::optional<Passed> taken;
    {
        const std::lock_guard lock{mutex_};
        const auto found = std::ranges::find(asks_, askerId, [](const Passed& passed) {
            return passed.asker.id;
        });
        if (found == asks_.end()) {
            return;
        }
        taken = std::move(*found);
        asks_.erase(found);
    }
    const std::string& name = taken->asker.name;
    const bool sent = hooks_.answer(
        taken->host.id, lan::wire::JoinAnswer{.asker = taken->asker.id, .allow = allow},
        [mutex = newsMutex_, news = news_, name](const std::string& why) {
            const std::lock_guard lock{*mutex};
            news->push_back("Your answer to " + name + " did not arrive: " + why);
        });
    if (!sent) {
        hooks_.say("Cannot answer " + name + " now: LAN is off or busy", true);
        return;
    }
    lucent::info("remote", "the player {} {} to join", allow ? "allows" : "declines", name);
    hooks_.say(allow ? name + " joins" : "Declined " + name, false);
}

} // namespace opensu::app

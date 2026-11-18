#include "busy_host.hpp"

#include <algorithm>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "library/shelf.hpp"

namespace opensu::app {

void BusyHost::ask(const library::Game& running, const library::Game& wanted,
                   const LaunchRequest& request) {
    const bool mine =
        std::ranges::find(running.playingAs, request.profile) != running.playingAs.end();
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(
        gpu2d::ui::Dialog::Choice{.label = mine ? "Continue it here" : "Ask to join it",
                                  .chosen =
                                      [this, running, request] {
                                          hooks_.launch(running, request);
                                      },
                                  .kind = gpu2d::ui::ButtonKind::Normal,
                                  .exits = false});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Close it and play " + wanted.title,
                                                .chosen =
                                                    [this, running, wanted, request] {
                                                        close(running, wanted, request);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = false});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Cancel",
                                                .chosen =
                                                    [] {
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        running.hostName + " is playing " + running.title,
        mine ? "It plays as your profile there, so it can move here and go on where it is, or "
               "close for " +
                   wanted.title + "."
             : "Its player there is asked whether you may join it, or close it for " +
                   wanted.title + ".",
        std::move(choices)));
}

void BusyHost::close(const library::Game& running, const library::Game& wanted,
                     const LaunchRequest& request) {
    if (pending_) {
        hooks_.say("Still asking " + pending_->running.hostName + " to close its game", true);
        return;
    }
    auto heard = std::make_shared<Heard>();
    const bool asked = lan_.askPeers(
        [heard, host = running.hostId, game = running.sourceId,
         profile = request.profile](const lan::LanNode& node, const std::stop_token& stop) {
            std::string error;
            const bool closed = node.askToClose(
                stop, host, game, profile,
                [heard] {
                    const std::lock_guard lock{heard->mutex};
                    heard->waiting = true;
                },
                error);
            if (stop.stop_requested()) {
                return;
            }
            const std::lock_guard lock{heard->mutex};
            heard->closed = closed;
            if (!closed) {
                heard->refusal = error.empty() ? std::string{"it did not close"} : error;
            }
        });
    if (!asked) {
        hooks_.say("Cannot reach " + running.hostName + " now", true);
        return;
    }
    hooks_.say("Asking " + running.hostName + " to close " + running.title, false);
    pending_ = Pending{.running = running,
                       .wanted = wanted,
                       .request = request,
                       .heard = std::move(heard),
                       .closedAt = std::nullopt};
}

void BusyHost::service(const std::vector<library::Game>& games) {
    if (!pending_) {
        return;
    }
    Pending& pending = *pending_;
    const std::string& host = pending.running.hostName;
    if (!pending.closedAt) {
        const std::lock_guard lock{pending.heard->mutex};
        if (pending.heard->waiting) {
            pending.heard->waiting = false;
            hooks_.say(host + "'s player is asked to close " + pending.running.title, false);
        }
        if (pending.heard->refusal) {
            hooks_.say(host + " did not close " + pending.running.title + ": " +
                           *pending.heard->refusal,
                       true);
            pending_.reset();
            return;
        }
        if (!pending.heard->closed) {
            return;
        }
        pending.closedAt = Clock::now();
        hooks_.say(host + " closed " + pending.running.title, false);
    }
    if (library::playingOn(games, pending.running.hostId) == nullptr) {
        const Pending done = std::move(pending);
        pending_.reset();
        hooks_.launch(done.wanted, done.request);
        return;
    }
    if (Clock::now() - *pending.closedAt > listingPatience) {
        hooks_.say(host + " still lists " + pending.running.title + " as playing", true);
        pending_.reset();
    }
}

} // namespace opensu::app

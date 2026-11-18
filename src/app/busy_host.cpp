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
    if (mine) {
        choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Close it and play " + wanted.title,
                                                    .chosen =
                                                        [this, running, wanted, request] {
                                                            close(running, wanted, request);
                                                        },
                                                    .kind = gpu2d::ui::ButtonKind::Normal,
                                                    .exits = false});
    }
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Cancel",
                                                .chosen =
                                                    [] {
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        running.hostName + " is playing " + running.title,
        mine ? "You are playing it there. Continue it here and it goes on where it is; close it "
               "and a game on a built-in core is saved first."
             : "Another profile is playing it. Ask to join it and its player there is asked.",
        std::move(choices)));
}

void BusyHost::close(const library::Game& running, const library::Game& wanted,
                     const LaunchRequest& request) {
    if (pending_) {
        hooks_.say("Still closing " + pending_->running.title, true);
        return;
    }
    auto heard = std::make_shared<Heard>();
    const bool asked = lan_.askPeers(
        [heard, host = running.hostId, game = running.sourceId,
         profile = request.profile](const lan::LanNode& node, const std::stop_token& stop) {
            std::string error;
            const bool closed = node.askToClose(stop, host, game, profile, error);
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
    hooks_.say("Closing " + running.title + " on " + running.hostName, false);
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
        Pending done = std::move(pending);
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

#include "giveaways_controller.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <utility>

#include "giveaways_screen.hpp"
#include "ui/interface_input.hpp"

namespace opensu::app {
namespace {

bool ours(const gpu2d::ui::Screen* screen) {
    return dynamic_cast<const GiveawaysScreen*>(screen) != nullptr ||
           dynamic_cast<const GiveawayDetails*>(screen) != nullptr;
}

} // namespace

GiveawaysController::GiveawaysController(Parts parts) : parts_{std::move(parts)} {
}

giveaways::Seconds GiveawaysController::now() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void GiveawaysController::open() {
    if (isOpen()) {
        return;
    }
    rebuild();
    parts_.navigator.push(std::make_unique<GiveawaysScreen>(*this, parts_.chrome, parts_.images));
}

void GiveawaysController::close() {
    while (ours(parts_.navigator.top())) {
        parts_.navigator.pop();
    }
}

bool GiveawaysController::isOpen() const {
    const std::vector<const gpu2d::ui::Screen*> open = parts_.navigator.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const GiveawaysScreen*>(screen) != nullptr;
    });
}

void GiveawaysController::act(gamepad::Button button) {
    if (const std::optional<gpu2d::ui::Action> action = ui::interfaceActionOf(button)) {
        parts_.navigator.act(*action);
    }
}

void GiveawaysController::refresh() {
    parts_.board.refresh();
    parts_.say("Checking the stores for free games", false);
}

void GiveawaysController::service() {
    if (const std::uint64_t generation = parts_.board.generation(); generation != generation_) {
        generation_ = generation;
        board_ = parts_.board.board();
    }
    rebuild();
    announce();
    for (const OfferClaim::News& news : parts_.claim.take()) {
        parts_.say(news.text, news.error);
    }
}

void GiveawaysController::rebuild() {
    shown_ = giveaways::shownOffers(
        board_, parts_.games(),
        giveaways::ListRules{parts_.turnedOn(), now()});
}

void GiveawaysController::announce() {
    if (board_.fetched == 0 || board_.reading) {
        return;
    }
    const giveaways::Seconds at = now();
    std::set<std::string> free;
    std::vector<const giveaways::Offer*> fresh;
    for (const giveaways::ShownOffer& shown : shown_) {
        if (giveaways::upcoming(shown.offer, at)) {
            continue;
        }
        const std::string key = giveaways::key(shown.offer);
        free.insert(key);
        if (known_ && !known_->contains(key) && !shown.owned) {
            fresh.push_back(&shown.offer);
        }
    }
    // The first read seen is where the player is; only what comes after it is news.
    if (known_ && !fresh.empty()) {
        std::string text = "Free to keep now: " + fresh.front()->title + " on " +
                           std::string{library::label(fresh.front()->store)};
        if (fresh.size() > 1) {
            text += " and " + std::to_string(fresh.size() - 1) + " more";
        }
        parts_.say(std::move(text), false);
    }
    if (known_) {
        known_->insert(free.begin(), free.end());
    } else {
        known_ = std::move(free);
    }
}

const giveaways::ShownOffer* GiveawaysController::find(const std::string& key) const {
    const auto found = std::ranges::find_if(shown_, [&key](const giveaways::ShownOffer& shown) {
        return giveaways::key(shown.offer) == key;
    });
    return found == shown_.end() ? nullptr : &*found;
}

std::string GiveawaysController::status() const {
    std::string text;
    if (board_.reading) {
        text = "Checking the stores";
    } else if (board_.fetched == 0) {
        text = "Not checked yet";
    } else {
        text = "Checked " + giveaways::spanText(now() - board_.fetched) + " ago";
    }
    for (const std::string& failure : board_.failures) {
        text += " · " + failure.substr(0, failure.find(':')) + " did not answer";
    }
    return text;
}

std::string GiveawaysController::lineOf(const giveaways::Offer& offer) const {
    return std::string{library::label(offer.store)} + " · " + giveaways::whenText(offer, now());
}

void GiveawaysController::details(const std::string& key) {
    parts_.navigator.push(
        std::make_unique<GiveawayDetails>(*this, parts_.chrome, parts_.images, key));
}

void GiveawaysController::claim(const std::string& key) {
    if (const giveaways::ShownOffer* shown = find(key)) {
        parts_.claim.claim(shown->offer);
    }
}

std::string GiveawaysController::openOffer() const {
    const auto* details = dynamic_cast<const GiveawayDetails*>(parts_.navigator.top());
    if (details == nullptr) {
        return {};
    }
    const giveaways::ShownOffer* shown = find(details->offer());
    return shown == nullptr ? std::string{} : shown->offer.title;
}

} // namespace opensu::app

#include "offer_claim.hpp"

#include <chrono>
#include <optional>
#include <utility>

#include "launch/command.hpp"
#include "lucent/log.h"
#include "steam/store_page.hpp"

namespace opensu::app {
namespace {

constexpr std::chrono::seconds browserTimeout{60};

std::string storeName(const giveaways::Offer& offer) {
    return std::string{library::label(offer.store)};
}

} // namespace

OfferClaim::OfferClaim(Parts parts)
    : parts_{std::move(parts)}, worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

OfferClaim::~OfferClaim() {
    worker_.request_stop();
    wake_.notify_all();
}

void OfferClaim::claim(const giveaways::Offer& offer) {
    launch::SteamGate* steam = parts_.steam();
    // Only openSU's own Steam, ready now, is asked; a claim never starts Steam.
    const bool inSteam = offer.store == library::Source::Steam && steam != nullptr &&
                         steam->ownsClient() && steam->state() == launch::SteamState::Ready;
    lucent::info("giveaways", "claim {} ({}) in {}", offer.title, giveaways::key(offer),
                 inSteam ? "Steam's Big Picture" : "the browser");
    {
        const std::lock_guard lock{mutex_};
        queue_.push_back(Job{offer, inSteam});
    }
    wake_.notify_all();
}

std::vector<OfferClaim::News> OfferClaim::take() {
    const std::lock_guard lock{mutex_};
    return std::exchange(news_, {});
}

void OfferClaim::tell(News news) {
    const std::lock_guard lock{mutex_};
    news_.push_back(std::move(news));
}

void OfferClaim::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Job job;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        perform(job, stop);
    }
}

void OfferClaim::perform(const Job& job, const std::stop_token& stop) {
    const giveaways::Offer& offer = job.offer;
    if (job.steam) {
        tell({"Opening " + offer.title + " in Steam", false});
        inSteam_ = true;
        steam::StorePage page{parts_.devToolsPort};
        std::string error;
        const bool shown = page.show(offer.pageUrl, stop, error);
        inSteam_ = false;
        if (shown) {
            return;
        }
        lucent::warn("giveaways", "Steam's store page: {}", error);
        if (stop.stop_requested()) {
            return;
        }
    }
    const std::optional<int> status =
        launch::runCommand(parts_.browser, {offer.pageUrl}, browserTimeout);
    if (!status || *status != 0) {
        lucent::warn("giveaways", "the browser did not open {}", offer.pageUrl);
        tell({"Cannot open a browser with " + parts_.browser, true});
        return;
    }
    tell({storeName(offer) + "'s page for " + offer.title + " is open in your browser", false});
}

} // namespace opensu::app

// offer_claim — what Claim on a giveaway does, only ever when the player presses it: the offer's
// store page opens for the player to claim it there. A Steam offer opens in Big Picture's browser
// on openSU's own Steam when that Steam is up and logged on (`steam::StorePage`), the shell
// handing Steam the screen and pads until the player leaves the page; otherwise, and for Epic and
// GOG, the page opens in the default browser. Nothing is claimed or bought here. GOG's own claim
// call (`POST www.gog.com/giveaway/claim`) takes the web store's session token, which openSU's
// Galaxy sign-in is not, so GOG opens its page too. Each claim runs on one worker; the loop takes
// what to tell the player with `take`.
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "giveaways/offer.hpp"
#include "launch/steam_gate.hpp"
#include "steam/devtools.hpp"

namespace opensu::app {

class OfferClaim {
  public:
    struct Parts {
        /// Opens a URL in the player's default browser.
        std::string browser{"xdg-open"};
        std::uint16_t devToolsPort{steam::devToolsPort};
        /// The Steam client in force, asked on the loop at each claim; null when there is none.
        std::function<launch::SteamGate*()> steam{[] {
            return nullptr;
        }};
    };

    struct News {
        std::string text;
        bool error{false};
    };

    explicit OfferClaim(Parts parts);
    ~OfferClaim();
    OfferClaim(const OfferClaim&) = delete;
    OfferClaim& operator=(const OfferClaim&) = delete;
    OfferClaim(OfferClaim&&) = delete;
    OfferClaim& operator=(OfferClaim&&) = delete;

    /// Opens `offer`'s store page. Loop only.
    void claim(const giveaways::Offer& offer);
    /// Whether a store page shows in Steam's Big Picture, whose windows then have the screen.
    [[nodiscard]] bool inSteam() const noexcept {
        return inSteam_.load();
    }
    /// What happened since the last call. Loop only.
    [[nodiscard]] std::vector<News> take();

  private:
    struct Job {
        giveaways::Offer offer;
        /// Open it in openSU's own Steam.
        bool steam{false};
    };

    void run(const std::stop_token& stop);
    void perform(const Job& job, const std::stop_token& stop);
    void tell(News news);

    Parts parts_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Job> queue_;
    std::vector<News> news_;
    std::atomic<bool> inSteam_{false};
    /// Last, so the worker stops before anything it uses is destroyed.
    std::jthread worker_;
};

} // namespace opensu::app

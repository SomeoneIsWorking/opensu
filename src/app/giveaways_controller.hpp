// giveaways_controller — the Giveaways screens' owner: opens them over the shell (the Library
// tile), keeps the list they show from `giveaways::GiveawayBoard` and the player's library and
// stores (`giveaways::shownOffers`), says how fresh it is, refreshes it on X, and hands Claim to
// `OfferClaim`. Each frame it tells the player of an offer that a refresh brought (one toast) and
// of what a claim did.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "giveaways/giveaway_board.hpp"
#include "giveaways/offer_list.hpp"
#include "offer_claim.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class GiveawaysController {
  public:
    struct Parts {
        gpu2d::ui::Navigator& navigator;
        const ui::Chrome& chrome;
        ui::ScreenImages& images;
        giveaways::GiveawayBoard& board;
        OfferClaim& claim;
        std::function<const std::vector<library::Game>&()> games;
        /// The stores the player has turned on.
        std::function<std::vector<library::Source>()> turnedOn;
        /// A toast.
        std::function<void(std::string text, bool error)> say;
    };

    explicit GiveawaysController(Parts parts);

    void open();
    /// Closes its screens.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Each frame: the list again from the board, the library and the settings; the news.
    void service();
    /// Asks the stores again.
    void refresh();

    /// The offers the list shows, in order.
    [[nodiscard]] const std::vector<giveaways::ShownOffer>& offers() const noexcept {
        return shown_;
    }
    [[nodiscard]] const giveaways::ShownOffer* find(const std::string& key) const;
    /// How fresh the offers are, and which store failed.
    [[nodiscard]] std::string status() const;
    /// The offer's line under its title: its store and when it ends.
    [[nodiscard]] std::string lineOf(const giveaways::Offer& offer) const;
    /// Opens the details of the offer `key`.
    void details(const std::string& key);
    void claim(const std::string& key);
    /// The title of the offer whose details are open; empty when none is.
    [[nodiscard]] std::string openOffer() const;

  private:
    void rebuild();
    /// Tells the player of offers free now that were not before.
    void announce();
    [[nodiscard]] giveaways::Seconds now() const;

    Parts parts_;
    std::uint64_t generation_{UINT64_MAX};
    giveaways::Board board_;
    std::vector<giveaways::ShownOffer> shown_;
    /// The offers free now as last seen; nothing until the stores were first read.
    std::optional<std::set<std::string>> known_;
};

} // namespace opensu::app

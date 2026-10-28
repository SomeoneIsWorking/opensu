// offer_list — which offers the Giveaways screen shows and in what order: every offer not ended,
// the stores the player has turned on first (in Steam, GOG, Epic order), then the others; free now
// before upcoming, the one ending soonest first. Each says whether the player owns it and when it
// ends, in words.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "giveaway_board.hpp"
#include "library/game.hpp"

namespace opensu::giveaways {

struct ShownOffer {
    Offer offer;
    /// The player's library has it in the same store.
    bool owned{false};
    /// Its art on disk; empty while it has none.
    std::filesystem::path art;
};

struct ListRules {
    /// The stores the player has turned on.
    std::vector<library::Source> turnedOn;
    Seconds now{0};
};

/// Whether `games` holds the offer's game in the offer's store: by the store's id, else by title.
[[nodiscard]] bool owned(const Offer& offer, const std::vector<library::Game>& games);

[[nodiscard]] std::vector<ShownOffer>
shownOffers(const Board& board, const std::vector<library::Game>& games, const ListRules& rules);

/// A length of time in its largest whole unit, at least a minute: "3 days", "5 hours", "1 minute".
[[nodiscard]] std::string spanText(Seconds seconds);

/// "Ends in 3 days", "Ends in 5 hours", "Free in 2 days" for an upcoming one, "No end date"
/// when the store gives none.
[[nodiscard]] std::string whenText(const Offer& offer, Seconds now);

} // namespace opensu::giveaways

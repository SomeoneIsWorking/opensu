// epic_offers — Epic's free games from the store's public promotions listing
// (`freeGamesPromotions`, no sign-in): each element whose current or upcoming promotional offer
// takes it to nothing (`discountPercentage` 0) is an offer, free from that offer's start to its
// end.
#pragma once

#include <string_view>
#include <vector>

#include "offer.hpp"

namespace opensu::giveaways::epic {

inline constexpr std::string_view promotionsUrl =
    "https://store-site-backend-static-ipv4.ak.epicgames.com/freeGamesPromotions"
    "?locale=en-US&country=US&allowCountries=US";

/// The free offers in a promotions answer, each at its first free window. Throws
/// `std::runtime_error` when the answer is not the listing's shape.
[[nodiscard]] std::vector<Offer> parsePromotions(std::string_view body);

} // namespace opensu::giveaways::epic

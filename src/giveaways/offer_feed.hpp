// offer_feed — one read of every store's free-to-keep offers over HTTPS, each store on its own:
// a store that fails says why and the others still count.
#pragma once

#include <stop_token>
#include <string>
#include <vector>

#include "net/web_client.hpp"
#include "offer.hpp"

namespace opensu::giveaways {

struct StoreRead {
    library::Source store{library::Source::Steam};
    /// Empty when the store answered.
    std::string error;
    std::vector<Offer> offers;
};

/// Epic's, GOG's and Steam's offers now, in that order. Blocks; run it off the frame loop.
[[nodiscard]] std::vector<StoreRead> readStores(const net::WebClient& web,
                                                const std::stop_token& stop);

} // namespace opensu::giveaways

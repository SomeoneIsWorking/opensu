#include "offer_feed.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "epic_offers.hpp"
#include "gog_offers.hpp"
#include "lucent/log.h"
#include "steam_offers.hpp"

namespace opensu::giveaways {
namespace {

// Steam leaves adult-only apps out of an anonymous search without a mature-content preference.
constexpr std::string_view steamCookie = "Cookie: wants_mature_content=1; birthtime=0";

/// The body of `url`; throws with the store's name and why when there is none.
std::string fetch(const net::WebClient& web, const std::string& url,
                  const std::vector<std::string>& headers = {}) {
    std::string error;
    std::optional<std::string> body = web.get(url, error, headers);
    if (!body) {
        throw std::runtime_error{error};
    }
    return std::move(*body);
}

std::vector<Offer> readEpic(const net::WebClient& web) {
    return epic::parsePromotions(fetch(web, std::string{epic::promotionsUrl}));
}

std::vector<Offer> readGog(const net::WebClient& web) {
    std::vector<Offer> offers;
    if (const std::optional<std::string> section =
            gog::giveawaySection(fetch(web, std::string{gog::homePageUrl}))) {
        const gog::Giveaway giveaway = gog::parseGiveaway(fetch(web, gog::sectionUrl(*section)));
        offers.push_back(gog::giveawayOffer(
            giveaway, gog::parseCatalog(fetch(web, gog::searchUrl(giveaway.title)))));
    }
    for (const gog::Product& product :
         gog::parseCatalog(fetch(web, std::string{gog::discountedFreeUrl}))) {
        if (std::optional<Offer> offer = gog::discountedToFree(product);
            offer && std::ranges::find(offers, offer->id, &Offer::id) == offers.end()) {
            offers.push_back(std::move(*offer));
        }
    }
    return offers;
}

std::vector<Offer> readSteam(const net::WebClient& web) {
    const std::vector<std::uint32_t> apps =
        steam::parseSearch(fetch(web, std::string{steam::searchUrl}, {std::string{steamCookie}}));
    std::vector<Offer> offers;
    if (apps.empty()) {
        return offers;
    }
    for (const steam::Item& item :
         steam::parseItems(fetch(web, steam::itemsUrl(apps), {std::string{steamCookie}}))) {
        if (std::optional<Offer> offer = steam::freeToKeep(item)) {
            offers.push_back(std::move(*offer));
        }
    }
    return offers;
}

} // namespace

std::vector<StoreRead> readStores(const net::WebClient& web, const std::stop_token& stop) {
    using Reader = std::vector<Offer> (*)(const net::WebClient&);
    const std::array<std::pair<library::Source, Reader>, 3> readers{
        std::pair{library::Source::Epic, &readEpic},
        std::pair{library::Source::Gog, &readGog},
        std::pair{library::Source::Steam, &readSteam},
    };
    std::vector<StoreRead> reads;
    for (const auto& [store, reader] : readers) {
        if (stop.stop_requested()) {
            break;
        }
        StoreRead read{store, {}, {}};
        try {
            read.offers = reader(web);
            lucent::info("giveaways", "{}: {} free to keep", library::label(store),
                         read.offers.size());
        } catch (const std::exception& error) {
            read.error = error.what();
            lucent::warn("giveaways", "{}: {}", library::label(store), read.error);
        }
        reads.push_back(std::move(read));
    }
    return reads;
}

} // namespace opensu::giveaways

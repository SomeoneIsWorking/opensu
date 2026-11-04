#include "offer.hpp"

#include <algorithm>
#include <array>

#include <nlohmann/json.hpp>

namespace opensu::giveaways {
namespace {

using nlohmann::json;

constexpr std::array cachedStores{library::Source::Steam, library::Source::Epic,
                                  library::Source::Gog};

} // namespace

std::string key(const Offer& offer) {
    return library::key(offer.store) + ":" + offer.id;
}

bool upcoming(const Offer& offer, Seconds now) noexcept {
    return offer.starts > now;
}

bool ended(const Offer& offer, Seconds now) noexcept {
    return offer.ends != 0 && offer.ends <= now;
}

std::string toJson(const std::vector<Offer>& offers, Seconds fetched) {
    json list = json::array();
    for (const Offer& offer : offers) {
        list.push_back({{"store", library::key(offer.store)},
                        {"id", offer.id},
                        {"title", offer.title},
                        {"dlc", offer.kind == OfferKind::Dlc},
                        {"art", offer.artUrl},
                        {"page", offer.pageUrl},
                        {"starts", offer.starts},
                        {"ends", offer.ends}});
    }
    return json{{"fetched", fetched}, {"offers", std::move(list)}}.dump(2);
}

std::optional<Cached> fromJson(std::string_view text) {
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object() || !document.contains("offers") || !document["offers"].is_array()) {
        return std::nullopt;
    }
    Cached cached;
    cached.fetched = document.value("fetched", Seconds{0});
    for (const json& entry : document["offers"]) {
        if (!entry.is_object()) {
            continue;
        }
        const std::string store = entry.value("store", "");
        const auto known = std::ranges::find_if(cachedStores, [&store](library::Source source) {
            return library::key(source) == store;
        });
        Offer offer;
        offer.id = entry.value("id", "");
        offer.title = entry.value("title", "");
        if (known == cachedStores.end() || offer.id.empty() || offer.title.empty()) {
            continue;
        }
        offer.store = *known;
        offer.kind = entry.value("dlc", false) ? OfferKind::Dlc : OfferKind::Game;
        offer.artUrl = entry.value("art", "");
        offer.pageUrl = entry.value("page", "");
        offer.starts = entry.value("starts", Seconds{0});
        offer.ends = entry.value("ends", Seconds{0});
        cached.offers.push_back(std::move(offer));
    }
    return cached;
}

} // namespace opensu::giveaways

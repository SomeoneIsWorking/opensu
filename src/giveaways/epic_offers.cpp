#include "epic_offers.hpp"

#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace opensu::giveaways::epic {
namespace {

using nlohmann::json;

constexpr std::string_view storePage = "https://store.epicgames.com/en-US/p/";

/// The first window in `groups` (`promotionalOffers` or `upcomingPromotionalOffers`) that makes
/// the game free.
std::optional<std::pair<Seconds, Seconds>> freeWindow(const json& groups) {
    if (!groups.is_array()) {
        return std::nullopt;
    }
    for (const json& group : groups) {
        const json offers = group.value("promotionalOffers", json::array());
        if (!offers.is_array()) {
            continue;
        }
        for (const json& offer : offers) {
            const json setting = offer.value("discountSetting", json::object());
            // The percentage is what is left to pay: 0 is free.
            if (!setting.is_object() || setting.value("discountPercentage", -1) != 0) {
                continue;
            }
            const std::optional<Seconds> starts = parseIsoTime(offer.value("startDate", ""));
            const std::optional<Seconds> ends = parseIsoTime(offer.value("endDate", ""));
            if (starts && ends) {
                return std::pair{*starts, *ends};
            }
        }
    }
    return std::nullopt;
}

/// The product page's slug: the product home mapping, else the product slug.
std::string pageSlug(const json& element) {
    for (const char* field : {"catalogNs", "offerMappings"}) {
        json mappings = element.value(field, json{});
        if (mappings.is_object()) {
            mappings = mappings.value("mappings", json{});
        }
        if (!mappings.is_array()) {
            continue;
        }
        for (const json& mapping : mappings) {
            if (mapping.value("pageType", "") == "productHome") {
                const std::string slug = mapping.value("pageSlug", "");
                if (!slug.empty()) {
                    return slug;
                }
            }
        }
    }
    const json product = element.value("productSlug", json{});
    std::string slug = product.is_string() ? product.get<std::string>() : "";
    if (const std::size_t home = slug.find('/'); home != std::string::npos) {
        slug.resize(home);
    }
    return slug;
}

/// The wide image, else the tall one, else the thumbnail.
std::string artOf(const json& element) {
    const json images = element.value("keyImages", json::array());
    if (!images.is_array()) {
        return {};
    }
    for (const char* type :
         {"OfferImageWide", "DieselStoreFrontWide", "OfferImageTall", "Thumbnail"}) {
        for (const json& image : images) {
            if (image.value("type", "") == type) {
                return image.value("url", "");
            }
        }
    }
    return {};
}

} // namespace

std::vector<Offer> parsePromotions(std::string_view body) {
    const json document = json::parse(body, nullptr, false);
    const json* elements = nullptr;
    if (document.is_object()) {
        const json::json_pointer path{"/data/Catalog/searchStore/elements"};
        if (document.contains(path) && document.at(path).is_array()) {
            elements = &document.at(path);
        }
    }
    if (elements == nullptr) {
        throw std::runtime_error{"Epic's promotions answer has an unexpected shape"};
    }
    std::vector<Offer> offers;
    for (const json& element : *elements) {
        const json promotions = element.value("promotions", json{});
        if (!element.is_object() || !promotions.is_object()) {
            continue;
        }
        std::optional<std::pair<Seconds, Seconds>> window =
            freeWindow(promotions.value("promotionalOffers", json{}));
        if (!window) {
            window = freeWindow(promotions.value("upcomingPromotionalOffers", json{}));
        }
        const std::string slug = pageSlug(element);
        Offer offer;
        offer.store = library::Source::Epic;
        offer.id = element.value("id", "");
        offer.title = element.value("title", "");
        if (!window || offer.id.empty() || offer.title.empty() || slug.empty()) {
            continue;
        }
        const std::string type = element.value("offerType", "");
        offer.kind = type == "DLC" || type == "ADD_ON" ? OfferKind::Dlc : OfferKind::Game;
        offer.artUrl = artOf(element);
        offer.pageUrl = std::string{storePage} + slug;
        offer.starts = window->first;
        offer.ends = window->second;
        offers.push_back(std::move(offer));
    }
    return offers;
}

} // namespace opensu::giveaways::epic

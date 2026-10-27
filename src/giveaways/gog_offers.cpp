#include "gog_offers.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "net/web_client.hpp"

namespace opensu::giveaways::gog {
namespace {

using nlohmann::json;

constexpr std::string_view sectionsApi = "https://sections.gog.com/v1/pages/2f/sections/";
constexpr std::string_view productPage = "https://www.gog.com/en/game/";
// The giveaway is claimed from the home page's banner.
constexpr std::string_view giveawayPage = "https://www.gog.com/en/#giveaway";

/// "24.99" as 2499; -1 when it is not an amount.
int cents(const json& money) {
    if (!money.is_object()) {
        return -1;
    }
    const std::string amount = money.value("amount", "");
    const std::size_t point = amount.find('.');
    const std::string whole = amount.substr(0, point);
    std::string fraction = point == std::string::npos ? "00" : amount.substr(point + 1);
    fraction.resize(2, '0');
    int units = 0;
    int hundredths = 0;
    if (whole.empty() ||
        std::from_chars(whole.data(), whole.data() + whole.size(), units).ec != std::errc{} ||
        std::from_chars(fraction.data(), fraction.data() + 2, hundredths).ec != std::errc{}) {
        return -1;
    }
    return units * 100 + hundredths;
}

std::string text(const json& object, const char* field) {
    const json value = object.value(field, json{});
    return value.is_string() ? value.get<std::string>() : std::string{};
}

} // namespace

std::string sectionUrl(std::string_view sectionId) {
    return std::string{sectionsApi} + net::percentEncode(sectionId) +
           "?countryCode=US&currencyCode=USD&locale=en-US";
}

std::string searchUrl(std::string_view title) {
    return "https://catalog.gog.com/v1/catalog?limit=10&query=like:" + net::percentEncode(title) +
           "&countryCode=US&locale=en-US&currencyCode=USD";
}

std::vector<Product> parseCatalog(std::string_view body) {
    const json document = json::parse(body, nullptr, false);
    if (!document.is_object() || !document.contains("products") ||
        !document["products"].is_array()) {
        throw std::runtime_error{"GOG's catalogue answer has an unexpected shape"};
    }
    std::vector<Product> products;
    for (const json& entry : document["products"]) {
        if (!entry.is_object()) {
            continue;
        }
        Product product;
        product.id = text(entry, "id");
        product.slug = text(entry, "slug");
        product.title = text(entry, "title");
        if (product.id.empty() || product.title.empty()) {
            continue;
        }
        product.type = text(entry, "productType");
        product.coverHorizontal = text(entry, "coverHorizontal");
        product.storeLink = text(entry, "storeLink");
        const json price = entry.value("price", json{});
        if (price.is_object()) {
            product.finalCents = cents(price.value("finalMoney", json{}));
            product.baseCents = cents(price.value("baseMoney", json{}));
        }
        products.push_back(std::move(product));
    }
    return products;
}

std::optional<Offer> discountedToFree(const Product& product) {
    if (product.finalCents != 0 || product.baseCents <= 0) {
        return std::nullopt;
    }
    Offer offer;
    offer.store = library::Source::Gog;
    offer.id = product.id;
    offer.title = product.title;
    offer.kind = product.type == "dlc" ? OfferKind::Dlc : OfferKind::Game;
    offer.artUrl = product.coverHorizontal;
    offer.pageUrl =
        product.storeLink.empty() ? std::string{productPage} + product.slug : product.storeLink;
    return offer;
}

std::optional<std::string> giveawaySection(std::string_view pageBody) {
    const json document = json::parse(pageBody, nullptr, false);
    if (!document.is_object() || !document.contains("sections") ||
        !document["sections"].is_array()) {
        throw std::runtime_error{"GOG's home page answer has an unexpected shape"};
    }
    for (const json& section : document["sections"]) {
        if (section.is_object() && text(section, "sectionType") == "GIVEAWAY_SECTION") {
            std::string id = text(section, "sectionId");
            if (!id.empty()) {
                return id;
            }
        }
    }
    return std::nullopt;
}

Giveaway parseGiveaway(std::string_view sectionBody) {
    const json document = json::parse(sectionBody, nullptr, false);
    const json properties = document.is_object() ? document.value("properties", json{}) : json{};
    const json product = properties.is_object() ? properties.value("product", json{}) : json{};
    if (!product.is_object() || text(product, "title").empty()) {
        throw std::runtime_error{"GOG's giveaway section names no product"};
    }
    Giveaway giveaway;
    giveaway.title = text(product, "title");
    giveaway.slug = text(product, "slug");
    giveaway.cover = text(product, "coverHorizontal");
    giveaway.ends = parseIsoTime(text(properties, "endDate")).value_or(0);
    return giveaway;
}

Offer giveawayOffer(const Giveaway& giveaway, const std::vector<Product>& found) {
    Offer offer;
    offer.store = library::Source::Gog;
    offer.id = giveaway.slug.empty() ? giveaway.title : giveaway.slug;
    offer.title = giveaway.title;
    offer.artUrl = giveaway.cover;
    offer.pageUrl = std::string{giveawayPage};
    offer.ends = giveaway.ends;
    const auto product = std::ranges::find(found, giveaway.slug, &Product::slug);
    if (!giveaway.slug.empty() && product != found.end()) {
        offer.id = product->id;
        offer.kind = product->type == "dlc" ? OfferKind::Dlc : OfferKind::Game;
        if (!product->coverHorizontal.empty()) {
            offer.artUrl = product->coverHorizontal;
        }
    }
    return offer;
}

} // namespace opensu::giveaways::gog

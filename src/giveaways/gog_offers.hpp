// gog_offers — GOG's free-to-keep games from its public APIs (no sign-in):
// - the catalogue (`catalog.gog.com/v1/catalog`), asked for products discounted to nothing; its
//   the giveaway's product is looked up there by title;
// - the giveaway, a `GIVEAWAY_SECTION` of the store's home page (`sections.gog.com/v1/pages/2f`,
//   the page key being the path "/" in hex), whose section names the product (title, slug, cover)
//   and when it ends.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "offer.hpp"

namespace opensu::giveaways::gog {

/// Games and packs discounted to nothing, US prices, most popular first.
inline constexpr std::string_view discountedFreeUrl =
    "https://catalog.gog.com/v1/catalog?limit=48&price=between:0,0&discounted=eq:true"
    "&productType=in:game,pack,dlc&order=desc:trending&page=1&countryCode=US&locale=en-US"
    "&currencyCode=USD";
inline constexpr std::string_view homePageUrl =
    "https://sections.gog.com/v1/pages/2f?countryCode=US&currencyCode=USD&locale=en-US";

/// The home page section `sectionId`.
[[nodiscard]] std::string sectionUrl(std::string_view sectionId);
/// The catalogue's products whose title is like `title`.
[[nodiscard]] std::string searchUrl(std::string_view title);

/// One catalogue product.
struct Product {
    std::string id;
    std::string slug;
    std::string title;
    /// "game", "pack" or "dlc".
    std::string type;
    std::string coverHorizontal;
    std::string storeLink;
    /// What it costs now and without a discount, in cents; -1 when not said.
    int finalCents{-1};
    int baseCents{-1};

    bool operator==(const Product&) const = default;
};

/// The products of a catalogue answer. Throws `std::runtime_error` when it is not the
/// catalogue's shape.
[[nodiscard]] std::vector<Product> parseCatalog(std::string_view body);

/// The product as an offer when it is discounted to nothing (free now, not before); the catalogue
/// does not say until when.
[[nodiscard]] std::optional<Offer> discountedToFree(const Product& product);

/// The giveaway section's id on the home page, or nothing when GOG runs none. Throws
/// `std::runtime_error` when the answer is not a page.
[[nodiscard]] std::optional<std::string> giveawaySection(std::string_view pageBody);

/// The giveaway as its section names it.
struct Giveaway {
    std::string title;
    std::string slug;
    std::string cover;
    Seconds ends{0};
};

/// The giveaway in a section answer. Throws `std::runtime_error` when it names no product.
[[nodiscard]] Giveaway parseGiveaway(std::string_view sectionBody);

/// The giveaway as an offer, with the catalogue's product of the same slug among `found` for its
/// id, kind and art when there is one.
[[nodiscard]] Offer giveawayOffer(const Giveaway& giveaway, const std::vector<Product>& found);

} // namespace opensu::giveaways::gog

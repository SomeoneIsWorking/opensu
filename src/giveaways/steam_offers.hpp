// steam_offers — Steam's limited-time free-to-keep games from its public store (no sign-in, no
// key): the store search for specials whose price is now nothing gives the app ids (a row with
// `data-discount="100"`), and `IStoreBrowseService/GetItems` says of each whether its best
// purchase option is free to keep (`is_free_to_keep`, a free-to-play game is not) and until when
// (`free_to_keep_ends`), and its art.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "offer.hpp"

namespace opensu::giveaways::steam {

/// Specials free now, US prices, every content preference ignored.
inline constexpr std::string_view searchUrl =
    "https://store.steampowered.com/search/results/?query=&start=0&count=100&maxprice=free"
    "&specials=1&infinite=1&cc=us&l=english&ignore_preferences=1";

/// The app ids of a search answer whose rows are discounted 100%. Throws `std::runtime_error`
/// when the answer is not the search's shape.
[[nodiscard]] std::vector<std::uint32_t> parseSearch(std::string_view body);

/// The `GetItems` request for `apps`.
[[nodiscard]] std::string itemsUrl(const std::vector<std::uint32_t>& apps);

/// One app as `GetItems` describes it.
struct Item {
    std::uint32_t app{0};
    std::string name;
    /// Steam's app type: 0 a game, 4 DLC.
    int type{-1};
    std::string storePath;
    std::string headerUrl;
    bool freeToKeep{false};
    Seconds freeToKeepEnds{0};

    bool operator==(const Item&) const = default;
};

/// The items of a `GetItems` answer. Throws `std::runtime_error` when it is not its shape.
[[nodiscard]] std::vector<Item> parseItems(std::string_view body);

/// The item as an offer when it is a game or DLC free to keep.
[[nodiscard]] std::optional<Offer> freeToKeep(const Item& item);

} // namespace opensu::giveaways::steam

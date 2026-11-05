// steam_items — what Steam's public `IStoreBrowseService/GetItems` says of an app, keyless: its
// name, type, purchase option and art. An app's art lives under hashed paths this answer alone
// gives; the plain `steam/apps/<id>/` CDN paths 404 for apps that have them.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::library::steam_items {

inline constexpr std::string_view itemsApi =
    "https://api.steampowered.com/IStoreBrowseService/GetItems/v1/";
inline constexpr std::string_view assetHost =
    "https://shared.akamai.steamstatic.com/store_item_assets/";

/// The `GetItems` request for `apps`, of the API at `api`.
[[nodiscard]] std::string itemsUrl(const std::vector<std::uint32_t>& apps,
                                   std::string_view api = itemsApi);

/// One app as `GetItems` describes it.
struct Item {
    std::uint32_t app{0};
    std::string name;
    /// Steam's app type: 0 a game, 4 DLC.
    int type{-1};
    std::string storePath;
    std::string headerUrl;
    /// Its library portrait, the larger one when it has both; empty when it has none.
    std::string portraitUrl;
    bool freeToKeep{false};
    /// Seconds since the epoch.
    std::int64_t freeToKeepEnds{0};

    bool operator==(const Item&) const = default;
};

/// The items of a `GetItems` answer, their art under `host`. Throws `std::runtime_error` when it is
/// not its shape.
[[nodiscard]] std::vector<Item> parseItems(std::string_view body,
                                           std::string_view host = assetHost);

} // namespace opensu::library::steam_items

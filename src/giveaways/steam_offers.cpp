#include "steam_offers.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "net/web_client.hpp"

namespace opensu::giveaways::steam {
namespace {

using nlohmann::json;

constexpr std::string_view itemsApi =
    "https://api.steampowered.com/IStoreBrowseService/GetItems/v1/";
constexpr std::string_view assetHost = "https://shared.akamai.steamstatic.com/store_item_assets/";
constexpr std::string_view storeHost = "https://store.steampowered.com/";
constexpr int gameType = 0;
constexpr int dlcType = 4;

/// The value of `attribute="..."` in `row`, or empty.
std::string_view attributeOf(std::string_view row, std::string_view attribute) {
    const std::string opening = std::string{attribute} + "=\"";
    const std::size_t at = row.find(opening);
    if (at == std::string_view::npos) {
        return {};
    }
    const std::size_t from = at + opening.size();
    const std::size_t to = row.find('"', from);
    return to == std::string_view::npos ? std::string_view{} : row.substr(from, to - from);
}

/// A number written as a JSON number or string.
std::int64_t numberOf(const json& value) {
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    std::int64_t number = 0;
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        static_cast<void>(std::from_chars(text.data(), text.data() + text.size(), number));
    }
    return number;
}

} // namespace

std::vector<std::uint32_t> parseSearch(std::string_view body) {
    const json document = json::parse(body, nullptr, false);
    if (!document.is_object() || !document.contains("results_html") ||
        !document["results_html"].is_string()) {
        throw std::runtime_error{"Steam's search answer has an unexpected shape"};
    }
    const std::string html = document["results_html"].get<std::string>();
    std::vector<std::uint32_t> apps;
    std::size_t at = html.find("<a ");
    while (at != std::string::npos) {
        const std::size_t next = html.find("<a ", at + 1);
        const std::string_view row = std::string_view{html}.substr(
            at, next == std::string::npos ? std::string::npos : next - at);
        const std::string_view app = attributeOf(row, "data-ds-appid");
        std::uint32_t id = 0;
        // A bundle's row lists several apps; only a single app is one offer.
        if (attributeOf(row, "data-discount") == "100" && !app.empty() &&
            std::from_chars(app.data(), app.data() + app.size(), id).ptr ==
                app.data() + app.size() &&
            std::ranges::find(apps, id) == apps.end()) {
            apps.push_back(id);
        }
        at = next;
    }
    return apps;
}

std::string itemsUrl(const std::vector<std::uint32_t>& apps) {
    json ids = json::array();
    for (const std::uint32_t app : apps) {
        ids.push_back({{"appid", app}});
    }
    const json input{{"ids", std::move(ids)},
                     {"context", {{"language", "english"}, {"country_code", "US"}}},
                     {"data_request", {{"include_assets", true}}}};
    return std::string{itemsApi} + "?input_json=" + net::percentEncode(input.dump());
}

std::vector<Item> parseItems(std::string_view body) {
    const json document = json::parse(body, nullptr, false);
    const json::json_pointer path{"/response/store_items"};
    if (!document.is_object() || !document.contains(path) || !document.at(path).is_array()) {
        throw std::runtime_error{"Steam's GetItems answer has an unexpected shape"};
    }
    std::vector<Item> items;
    for (const json& entry : document.at(path)) {
        if (!entry.is_object() || entry.value("success", 0) != 1) {
            continue;
        }
        Item item;
        item.app = static_cast<std::uint32_t>(numberOf(entry.value("appid", json{})));
        item.name = entry.value("name", "");
        item.type = entry.value("type", -1);
        item.storePath = entry.value("store_url_path", "");
        const json assets = entry.value("assets", json{});
        if (assets.is_object()) {
            std::string format = assets.value("asset_url_format", "");
            const std::string header = assets.value("header", "");
            constexpr std::string_view slot = "${FILENAME}";
            if (const std::size_t at = format.find(slot);
                at != std::string::npos && !header.empty()) {
                item.headerUrl = std::string{assetHost} + format.replace(at, slot.size(), header);
            }
        }
        const json purchase = entry.value("best_purchase_option", json{});
        if (purchase.is_object()) {
            item.freeToKeep = purchase.value("is_free_to_keep", false);
            item.freeToKeepEnds = numberOf(purchase.value("free_to_keep_ends", json{}));
        }
        if (item.app != 0 && !item.name.empty()) {
            items.push_back(std::move(item));
        }
    }
    return items;
}

std::optional<Offer> freeToKeep(const Item& item) {
    if (!item.freeToKeep || (item.type != gameType && item.type != dlcType)) {
        return std::nullopt;
    }
    Offer offer;
    offer.store = library::Source::Steam;
    offer.id = std::to_string(item.app);
    offer.title = item.name;
    offer.kind = item.type == dlcType ? OfferKind::Dlc : OfferKind::Game;
    offer.artUrl = item.headerUrl;
    offer.pageUrl =
        std::string{storeHost} + (item.storePath.empty() ? "app/" + offer.id : item.storePath);
    offer.ends = item.freeToKeepEnds;
    return offer;
}

} // namespace opensu::giveaways::steam

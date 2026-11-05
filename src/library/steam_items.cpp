#include "steam_items.hpp"

#include <charconv>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "net/web_client.hpp"

namespace opensu::library::steam_items {
namespace {

using nlohmann::json;

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

/// The URL of asset `key` in an item's `assets`; empty when it has none.
std::string assetUrl(const json& assets, std::string_view host, const char* key) {
    std::string format = assets.value("asset_url_format", "");
    const std::string file = assets.value(key, "");
    constexpr std::string_view slot = "${FILENAME}";
    const std::size_t at = format.find(slot);
    if (at == std::string::npos || file.empty()) {
        return {};
    }
    return std::string{host} + format.replace(at, slot.size(), file);
}

} // namespace

std::string itemsUrl(const std::vector<std::uint32_t>& apps, std::string_view api) {
    json ids = json::array();
    for (const std::uint32_t app : apps) {
        ids.push_back({{"appid", app}});
    }
    const json input{{"ids", std::move(ids)},
                     {"context", {{"language", "english"}, {"country_code", "US"}}},
                     {"data_request", {{"include_assets", true}}}};
    return std::string{api} + "?input_json=" + net::percentEncode(input.dump());
}

std::vector<Item> parseItems(std::string_view body, std::string_view host) {
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
            item.headerUrl = assetUrl(assets, host, "header");
            item.portraitUrl = assetUrl(assets, host, "library_capsule_2x");
            if (item.portraitUrl.empty()) {
                item.portraitUrl = assetUrl(assets, host, "library_capsule");
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

} // namespace opensu::library::steam_items

#include "steam_offers.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace opensu::giveaways::steam {
namespace {

using nlohmann::json;

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

#include "offer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>

#include <nlohmann/json.hpp>

namespace opensu::giveaways {
namespace {

using nlohmann::json;

constexpr std::array cachedStores{library::Source::Steam, library::Source::Epic,
                                  library::Source::Gog};

/// The digits at `at` to `at + count` as a number, or -1.
int digits(std::string_view text, std::size_t at, std::size_t count) {
    if (at + count > text.size()) {
        return -1;
    }
    int value = 0;
    const char* first = text.data() + at;
    const auto [end, error] = std::from_chars(first, first + count, value);
    return error == std::errc{} && end == first + count ? value : -1;
}

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

std::optional<Seconds> parseIsoTime(std::string_view text) {
    // YYYY-MM-DDTHH:MM:SS
    constexpr std::size_t clockEnd = 19;
    if (text.size() < clockEnd || text[4] != '-' || text[7] != '-' ||
        (text[10] != 'T' && text[10] != ' ') || text[13] != ':' || text[16] != ':') {
        return std::nullopt;
    }
    const int year = digits(text, 0, 4);
    const int month = digits(text, 5, 2);
    const int day = digits(text, 8, 2);
    const int hour = digits(text, 11, 2);
    const int minute = digits(text, 14, 2);
    const int second = digits(text, 17, 2);
    if (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0 || second < 0) {
        return std::nullopt;
    }
    const std::chrono::year_month_day date{std::chrono::year{year},
                                           std::chrono::month{static_cast<unsigned>(month)},
                                           std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok() || hour > 23 || minute > 59 || second > 60) {
        return std::nullopt;
    }
    std::size_t at = clockEnd;
    if (at < text.size() && text[at] == '.') {
        ++at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            ++at;
        }
    }
    std::chrono::seconds offset{0};
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
        const int hours = digits(text, at + 1, 2);
        const bool colon = at + 3 < text.size() && text[at + 3] == ':';
        const int minutes = digits(text, at + (colon ? 4 : 3), 2);
        if (hours < 0 || minutes < 0) {
            return std::nullopt;
        }
        offset = (text[at] == '+' ? 1 : -1) *
                 (std::chrono::hours{hours} + std::chrono::minutes{minutes});
        at += colon ? 6 : 5;
    } else if (at < text.size() && text[at] == 'Z') {
        ++at;
    }
    if (at != text.size()) {
        return std::nullopt;
    }
    const std::chrono::sys_seconds moment = std::chrono::sys_days{date} + std::chrono::hours{hour} +
                                            std::chrono::minutes{minute} +
                                            std::chrono::seconds{second} - offset;
    return moment.time_since_epoch().count();
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

#include "offer_list.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>

#include "library/titles.hpp"

namespace opensu::giveaways {
namespace {

constexpr std::array storeOrder{library::Source::Steam, library::Source::Gog,
                                library::Source::Epic};
constexpr Seconds minute = 60;
constexpr Seconds hour = 60 * minute;
constexpr Seconds day = 24 * hour;

std::string count(Seconds amount, const char* unit) {
    return std::to_string(amount) + " " + unit + (amount == 1 ? "" : "s");
}

} // namespace

std::string spanText(Seconds seconds) {
    if (seconds >= 2 * day) {
        return count(seconds / day, "day");
    }
    if (seconds >= hour) {
        return count(seconds / hour, "hour");
    }
    return count(std::max<Seconds>(seconds / minute, 1), "minute");
}

namespace {

/// Where `store` stands: turned-on stores first, each group in the store order.
std::size_t rank(library::Source store, const std::vector<library::Source>& turnedOn) {
    const auto at =
        static_cast<std::size_t>(std::ranges::find(storeOrder, store) - storeOrder.begin());
    const bool on = std::ranges::find(turnedOn, store) != turnedOn.end();
    return (on ? 0 : storeOrder.size()) + at;
}

} // namespace

bool owned(const Offer& offer, const std::vector<library::Game>& games) {
    const std::string title = library::titleKey(offer.title);
    return std::ranges::any_of(games, [&offer, &title](const library::Game& game) {
        return game.source == offer.store &&
               (game.sourceId == offer.id ||
                (!title.empty() && library::titleKey(game.title) == title));
    });
}

std::vector<ShownOffer> shownOffers(const Board& board, const std::vector<library::Game>& games,
                                    const ListRules& rules) {
    std::vector<ShownOffer> shown;
    for (const Offer& offer : board.offers) {
        if (ended(offer, rules.now)) {
            continue;
        }
        const auto art = board.art.find(key(offer));
        shown.push_back(ShownOffer{offer, owned(offer, games),
                                   art == board.art.end() ? std::filesystem::path{} : art->second});
    }
    std::ranges::stable_sort(shown, [&rules](const ShownOffer& a, const ShownOffer& b) {
        const auto order = [&rules](const Offer& offer) {
            // An offer with no end date sorts after the ones that end.
            return std::tuple{rank(offer.store, rules.turnedOn), upcoming(offer, rules.now),
                              offer.ends == 0 ? std::numeric_limits<Seconds>::max() : offer.ends};
        };
        return order(a.offer) < order(b.offer);
    });
    return shown;
}

std::string whenText(const Offer& offer, Seconds now) {
    if (upcoming(offer, now)) {
        return "Free in " + spanText(offer.starts - now);
    }
    if (offer.ends == 0) {
        return "No end date";
    }
    return "Ends in " + spanText(offer.ends - now);
}

} // namespace opensu::giveaways

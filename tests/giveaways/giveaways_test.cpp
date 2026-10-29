// The stores' offer parsers against their saved answers (`fixtures/`, captured 2026-10-27 and
// trimmed), the cache's round trip and the list's rules.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "giveaways/epic_offers.hpp"
#include "giveaways/gog_offers.hpp"
#include "giveaways/offer.hpp"
#include "giveaways/offer_list.hpp"
#include "giveaways/steam_offers.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::giveaways;
using opensu::library::Game;
using opensu::library::Source;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::string fixture(const char* name) {
    std::ifstream file{std::string{OPENSU_GIVEAWAY_FIXTURES} + "/" + name};
    expect(file.good(), name);
    return std::string{std::istreambuf_iterator<char>{file}, {}};
}

constexpr Seconds hour = 3600;
constexpr Seconds day = 24 * hour;

/// The time `iso` names; fails the test when it does not read.
Seconds timeOf(const char* iso) {
    const std::optional<Seconds> time = parseIsoTime(iso);
    expect(time.has_value(), iso);
    return time.value_or(0);
}

const Offer* titled(const std::vector<Offer>& offers, const std::string& title) {
    const auto found = std::ranges::find(offers, title, &Offer::title);
    return found == offers.end() ? nullptr : &*found;
}

bool throws(void (*call)()) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void isoTimes() {
    expect(parseIsoTime("2026-10-29T15:00:00.000Z") == 1793286000, "Epic's time");
    expect(parseIsoTime("2026-10-29T15:00:00Z") == 1793286000, "no fraction");
    expect(parseIsoTime("2026-10-29T16:00:00+01:00") == 1793286000, "an offset");
    expect(parseIsoTime("2026-10-29T15:00:00") == 1793286000, "no zone is UTC");
    expect(!parseIsoTime("2026-13-29T15:00:00Z"), "no thirteenth month");
    expect(!parseIsoTime("tomorrow"), "not a time");
    expect(!parseIsoTime(""), "empty");
}

void epicPromotions() {
    const std::vector<Offer> offers = epic::parsePromotions(fixture("epic-promotions.json"));
    expect(offers.size() == 3, "Epic: two free now and one free later, not the discounted ones");
    const Offer* sker = titled(offers, "Sker Ritual");
    expect(sker != nullptr, "Epic: Sker Ritual");
    expect(sker->store == Source::Epic && sker->kind == OfferKind::Game, "Epic: a game");
    expect(sker->id == "6aab3ddef4f84fbc9dd81c60f9d8549c", "Epic: the offer id");
    expect(sker->pageUrl == "https://store.epicgames.com/en-US/p/sker-ritual-aac702",
           "Epic: the product home page");
    expect(sker->starts == timeOf("2026-10-22T15:00:00.000Z") &&
               sker->ends == timeOf("2026-10-29T15:00:00.000Z"),
           "Epic: the free window");
    expect(sker->artUrl.find("cdn1.epicgames.com") != std::string::npos, "Epic: the wide art");
    const Offer* closed = titled(offers, "Sorry We're Closed");
    expect(closed != nullptr, "Epic: the upcoming one");
    expect(closed->starts == timeOf("2026-10-29T15:00:00.000Z"),
           "Epic: upcoming from its free window, not the current 70% one");
    expect(upcoming(*closed, timeOf("2026-10-27T12:00:00Z")), "Epic: not free yet");
    expect(titled(offers, "Eternal Threads") == nullptr, "Epic: 20% off is not free");
    expect(titled(offers, "Them's Fightin' Herds") == nullptr, "Epic: no promotion");
    expect(throws([] {
               static_cast<void>(epic::parsePromotions("{\"data\":{}}"));
           }),
           "Epic: another shape throws");
}

void steamOffers() {
    const std::vector<std::uint32_t> apps = steam::parseSearch(fixture("steam-search.json"));
    expect(apps == std::vector<std::uint32_t>{5340370}, "Steam: the one row 100% off");
    expect(steam::itemsUrl(apps).find("input_json=%7B%22context") != std::string::npos,
           "Steam: the request is encoded");
    const std::vector<steam::Item> items = steam::parseItems(fixture("steam-items.json"));
    expect(items.size() == 2, "Steam: two items");
    std::vector<Offer> offers;
    for (const steam::Item& item : items) {
        if (std::optional<Offer> offer = steam::freeToKeep(item)) {
            offers.push_back(*offer);
        }
    }
    expect(offers.size() == 1, "Steam: only the free-to-keep one");
    const Offer& popcorn = offers.front();
    expect(popcorn.id == "5340370" && popcorn.kind == OfferKind::Dlc, "Steam: DLC by its type");
    expect(popcorn.ends == 1793293200, "Steam: free_to_keep_ends");
    expect(popcorn.pageUrl ==
               "https://store.steampowered.com/app/5340370/Popcorn_Premieren_Ausgehviertel",
           "Steam: the store page");
    expect(popcorn.artUrl ==
               "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/5340370/"
               "843485bcea45d0475504553f148de79e2cdf2da8/header.jpg?t=1792688582",
           "Steam: the header art");
}

void gogOffers() {
    const std::vector<gog::Product> discounted =
        gog::parseCatalog(fixture("gog-catalog-discounted.json"));
    expect(discounted.size() == 2, "GOG: two products");
    const auto product = [&discounted](const char* title) {
        return *std::ranges::find(discounted, std::string{title}, &gog::Product::title);
    };
    expect(product("Vampyr").finalCents == 399 && product("Vampyr").baseCents == 3999,
           "GOG: the prices in cents");
    expect(std::ranges::none_of(discounted,
                                [](const gog::Product& each) {
                                    return gog::discountedToFree(each).has_value();
                                }),
           "GOG: discounted but not free is no offer");

    const std::vector<gog::Product> free =
        gog::parseCatalog(fixture("gog-catalog-always-free.json"));
    expect(free.size() == 1 && free.front().finalCents == 0 && free.front().baseCents == 0,
           "GOG: an always-free product");
    expect(!gog::discountedToFree(free.front()), "GOG: always free is not a giveaway");
    expect(gog::parseCatalog(fixture("gog-catalog-discounted-free.json")).empty(),
           "GOG: none discounted to free that day");
    expect(!gog::giveawaySection(fixture("gog-home-page.json")),
           "GOG: no giveaway section that day");

    gog::Product made = product("Vampyr");
    made.finalCents = 0;
    const std::optional<Offer> offer = gog::discountedToFree(made);
    expect(offer && offer->id == "2098644729" && offer->pageUrl == made.storeLink,
           "GOG: a product at nothing is an offer");

    const Offer giveaway = gog::giveawayOffer(
        gog::Giveaway{"Vampyr", made.slug, "https://images.gog-statics.com/cover", 10}, discounted);
    expect(giveaway.id == "2098644729" && giveaway.ends == 10 &&
               giveaway.pageUrl == "https://www.gog.com/en/#giveaway",
           "GOG: the giveaway takes the catalogue product's id");
    const Offer unknown =
        gog::giveawayOffer(gog::Giveaway{"Unlisted", "unlisted", "", 0}, discounted);
    expect(unknown.id == "unlisted", "GOG: an unlisted giveaway");
    expect(throws([] {
               static_cast<void>(gog::parseGiveaway("{\"properties\":{}}"));
           }),
           "GOG: a section naming no product throws");
}

void cache() {
    const std::vector<Offer> offers = epic::parsePromotions(fixture("epic-promotions.json"));
    const std::optional<Cached> back = fromJson(toJson(offers, 42));
    expect(back && back->fetched == 42 && back->offers == offers, "cache: round trip");
    expect(!fromJson("not json"), "cache: unreadable");
}

void list() {
    const Seconds now = timeOf("2026-10-27T12:00:00Z");
    Board board;
    board.offers = {
        Offer{Source::Epic, "e1", "Sker Ritual", OfferKind::Game, "", "", 0, now + 2 * day},
        Offer{Source::Epic, "e2", "Later", OfferKind::Game, "", "", now + 3 * day, now + 9 * day},
        Offer{Source::Steam, "5", "Steam Game", OfferKind::Game, "", "", 0, now + 5 * hour},
        Offer{Source::Gog, "8", "Over", OfferKind::Game, "", "", 0, now - 1},
        Offer{Source::Gog, "9", "Open-ended", OfferKind::Game, "", "", 0, 0},
    };
    board.art.emplace("steam:5", "/art/steam_5");
    Game sker;
    sker.source = Source::Epic;
    sker.sourceId = "SkerApp";
    sker.title = "SKER RITUAL";
    Game other;
    other.source = Source::Steam;
    other.sourceId = "6";
    other.title = "Open-ended";

    const std::vector<ShownOffer> shown =
        shownOffers(board, {sker, other}, ListRules{{Source::Epic}, now});
    std::vector<std::string> titles;
    titles.reserve(shown.size());
    for (const ShownOffer& each : shown) {
        titles.push_back(each.offer.title);
    }
    expect(titles == std::vector<std::string>{"Sker Ritual", "Later", "Steam Game", "Open-ended"},
           "list: turned-on store first, now before upcoming, ended left out");
    expect(shown[0].owned, "list: owned by title in the same store");
    expect(!shown[3].owned, "list: the same title in another store is not owned");
    expect(shown[2].art == "/art/steam_5", "list: the art on disk");
    expect(whenText(board.offers[0], now) == "Ends in 2 days", "when: days");
    expect(whenText(board.offers[1], now) == "Free in 3 days", "when: upcoming");
    expect(whenText(board.offers[2], now) == "Ends in 5 hours", "when: hours");
    expect(whenText(board.offers[4], now) == "No end date", "when: none given");
}

} // namespace

int main() try {
    isoTimes();
    epicPromotions();
    steamOffers();
    gogOffers();
    cache();
    list();
    std::puts("giveaways: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

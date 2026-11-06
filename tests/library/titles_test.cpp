// Title merging across stores: the key titles are compared by, and which copies make one title.
#include "library/titles.hpp"
#include "support/test_main.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {

using opensu::library::copiesOf;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::storeTitles;
using opensu::library::TitleCopies;
using opensu::library::titleKey;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game game(Source source, std::string id, std::string title, bool installed = false) {
    Game out;
    out.source = source;
    out.id = std::move(id);
    out.title = std::move(title);
    out.installed = installed;
    return out;
}

void testKey() {
    expect(titleKey("Hades II") == "hadesii", "case and spaces do not matter");
    expect(titleKey("HADES  II") == titleKey("hades ii"), "runs of spaces do not matter");
    expect(titleKey("Hades II™") == "hadesii", "a trademark sign does not count");
    expect(titleKey("The Witcher® 3: Wild Hunt") == "thewitcher3wildhunt",
           "punctuation does not count");
    expect(titleKey("Brothers & Sisters") == titleKey("Brothers and Sisters"),
           "an ampersand reads as and");
    expect(titleKey("Doom 3") != titleKey("Doom"), "a number is part of the title");
    expect(titleKey("魔界戦記").empty(), "a title with no letters or digits has no key");
    expect(titleKey("").empty(), "an empty title has no key");
}

void testMerge() {
    const std::vector<Game> games{
        game(Source::Epic, "epic:a", "Alpha"),   game(Source::Steam, "steam:a", "ALPHA™", true),
        game(Source::Gog, "gog:a", "Alpha"),     game(Source::Steam, "steam:b", "Beta"),
        game(Source::Rom, "rom:alpha", "Alpha"), game(Source::Epic, "epic:c", "魔界戦記"),
        game(Source::Gog, "gog:c", "魔界戦記"),
    };
    const std::vector<opensu::library::Title> titles = storeTitles(games);
    expect(titles.size() == 4, "Alpha across three stores, Beta, and two unkeyed games");

    // Alpha: installed Steam first, then GOG, then Epic; the ROM is no store copy.
    expect(titles[0].copies.size() == 3, "three stores own Alpha");
    expect(titles[0].copies[0]->id == "steam:a" && titles[0].copies[1]->id == "gog:a" &&
               titles[0].copies[2]->id == "epic:a",
           "the installed copy first, then Steam, GOG, Epic");
    expect(titles[1].copies.size() == 1 && titles[1].copies[0]->id == "steam:b", "Beta is alone");
    expect(titles[2].copies.size() == 1 && titles[3].copies.size() == 1,
           "titles without a key are never merged");

    // Titles come in the catalog position of their preferred copy: steam:a is third, Beta fourth.
    expect(titles[0].copies[0] == &games[1] && titles[1].copies[0] == &games[3],
           "titles are ordered by their preferred copy's catalog position");
}

void testSameStore() {
    const std::vector<Game> games{
        game(Source::Steam, "steam:1", "Doom"),
        game(Source::Steam, "steam:2", "DOOM"),
        game(Source::Gog, "gog:1", "Doom"),
    };
    const std::vector<opensu::library::Title> titles = storeTitles(games);
    expect(titles.size() == 2, "two Steam games with one key stay two titles");
    expect(titles[0].copies.size() == 2 && titles[1].copies.size() == 1,
           "the other store joins the first of them");
}

void testCopiesOf() {
    const std::vector<Game> games{
        game(Source::Epic, "epic:a", "Alpha"),
        game(Source::Steam, "steam:a", "Alpha"),
        game(Source::Gog, "gog:b", "Beta"),
    };
    const std::vector<Game> alpha = copiesOf(games, games[0]);
    expect(alpha.size() == 2 && alpha[0].id == "steam:a" && alpha[1].id == "epic:a",
           "the copies of a title, preferred first");
    expect(copiesOf(games, games[2]).size() == 1, "a title in one store has one copy");
    const Game stranger = game(Source::Rom, "rom:x", "Alpha");
    expect(copiesOf(games, stranger).size() == 1 && copiesOf(games, stranger)[0].id == "rom:x",
           "a game outside the catalog's stores is its own copy");
}

/// Alpha: Steam not installed, GOG not installed, Epic installed. Beta is GOG's alone.
std::vector<Game> owned() {
    std::vector<Game> games{
        game(Source::Steam, "steam:a", "Alpha"),
        game(Source::Gog, "gog:a", "Alpha"),
        game(Source::Epic, "epic:a", "Alpha"),
        game(Source::Gog, "gog:b", "Beta"),
    };
    games[0].installed = false;
    games[1].installed = false;
    games[2].installed = true;
    return games;
}

/// Data in a map, as a store of it keeps it.
struct Kept {
    std::map<std::string, std::string> entries;

    [[nodiscard]] opensu::library::KeptData data() {
        return {[this](const std::string& id) {
                    return entries.contains(id);
                },
                [this](const std::string& from, const std::string& to) {
                    entries[to] = entries.at(from);
                }};
    }
};

void testSharedData() {
    using opensu::library::dataId;
    using opensu::library::shareTitleData;
    const auto at = [](int seconds) {
        return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
    };
    std::vector<Game> games = owned();
    Kept kept;
    kept.entries = {{"epic:a", "epic"}};
    shareTitleData(games, {kept.data()});
    expect(dataId(games[1]) == "title:alpha" && dataId(games[2]) == "title:alpha",
           "a title of two stores keeps its data under its key");
    expect(kept.entries.at("title:alpha") == "epic" && kept.entries.at("epic:a") == "epic",
           "starting from the copy that has some, which stays");
    expect(dataId(games[3]) == games[3].id, "a one-store game keeps its id");

    games = owned();
    games[1].lastPlayed = at(100);
    games[2].lastPlayed = at(200);
    kept.entries = {{games[1].id, "first"}, {games[2].id, "second"}};
    shareTitleData(games, {kept.data()});
    expect(kept.entries.at("title:alpha") == "second" && kept.entries.at(games[1].id) == "first",
           "of several the most recently played wins, the others untouched");

    kept.entries.at("title:alpha") = "changed";
    shareTitleData(games, {kept.data()});
    expect(kept.entries.at("title:alpha") == "changed", "what the key holds is not read over");

    games.erase(games.begin() + 2);
    shareTitleData(games, {kept.data()});
    expect(dataId(games[1]) == "title:alpha", "a title that lost a copy finds what it kept");

    games.back().title = "Alpha Renamed";
    games[1].title = "Alpha Renamed";
    shareTitleData(games, {kept.data()});
    expect(dataId(games[1]) == games[1].id, "a new title falls back to the copy's own");

    std::vector<Game> twins = owned();
    twins.push_back(game(Source::Steam, "steam:twin", "Alpha"));
    kept.entries = {{"title:alpha", "theirs"}};
    shareTitleData(twins, {kept.data()});
    expect(dataId(twins.back()) == "steam:twin",
           "a same-titled game of one store is not the title of two");
}

void testTopFollowsPlayHistory() {
    std::vector<Game> games = owned();
    const TitleCopies unplayed{games};
    expect(unplayed.top(games[0])->id == "epic:a", "never played: the installed copy leads");
    expect(unplayed.top(games[3])->id == "gog:b", "a title of one store is that copy");

    const auto now = std::chrono::system_clock::now();
    games[0].lastPlayed = now - std::chrono::hours{2};
    games[1].lastPlayed = now - std::chrono::hours{1};
    const TitleCopies played{games};
    const std::vector<const Game*> order = played.ordered(games[2]);
    expect(order.size() == 3 && order[0]->id == "epic:a" && order[1]->id == "gog:a" &&
               order[2]->id == "steam:a",
           "an installed copy leads any played one that is not, which go latest first");

    games[0].installed = true;
    games[2].lastPlayed = now - std::chrono::hours{3};
    const std::vector<const Game*> both = TitleCopies{games}.ordered(games[2]);
    expect(both[0]->id == "steam:a" && both[1]->id == "epic:a",
           "among the installed the copy last played leads");
    games[0].installed = false;
    expect(played.top(games[0]) == played.top(games[1]) &&
               played.top(games[1]) == played.top(games[2]),
           "every copy of the title has one top");

    const Game stranger = game(Source::Rom, "rom:x", "Alpha");
    expect(played.top(stranger) == nullptr && played.ordered(stranger).empty(),
           "a game of no title has no copy");
}

void testNeverPlayedOrder() {
    std::vector<Game> games = owned();
    games[0].installed = true;
    games[1].installed = false;
    games[2].installed = true;
    const std::vector<const Game*> order = TitleCopies{games}.ordered(games[1]);
    expect(order.size() == 3 && order[0]->id == "steam:a" && order[1]->id == "epic:a" &&
               order[2]->id == "gog:a",
           "installed first, then Steam, GOG, Epic within that");
}

void testOffered() {
    std::vector<Game> games = owned();
    games.push_back(game(Source::Own, "own:a", "Alpha"));
    games[4].installed = true;
    games[4].lastPlayed = std::chrono::system_clock::now();
    const std::vector<Game> stores = TitleCopies{games}.offered(games[0]);
    expect(stores.size() == 3 && stores[0].id == "epic:a" && stores[1].id == "steam:a" &&
               stores[2].id == "gog:a",
           "a button per store copy, top first, and a copy of no store never leads by play");
    expect(TitleCopies{games}.offered(games[3]).empty(), "one store has plain buttons");

    const std::vector<Game> oneStore{game(Source::Steam, "steam:c", "Gamma"),
                                     game(Source::Own, "own:c", "Gamma", true)};
    expect(TitleCopies{oneStore}.offered(oneStore[0]).empty(),
           "a store's copy and the player's own are one store");

    const std::vector<Game> ownLeads{game(Source::Steam, "steam:d", "Delta"),
                                     game(Source::Epic, "epic:d", "Delta"),
                                     game(Source::Own, "own:d", "Delta", true)};
    const std::vector<Game> led = TitleCopies{ownLeads}.offered(ownLeads[0]);
    expect(led.size() == 3 && led[0].id == "own:d",
           "the only installed copy tops the title and has its button");
}

} // namespace

int main() try {
    testKey();
    testMerge();
    testSameStore();
    testCopiesOf();
    testSharedData();
    testTopFollowsPlayHistory();
    testNeverPlayedOrder();
    testOffered();
    std::printf("titles: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

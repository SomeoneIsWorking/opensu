// Version grouping: the keys titles are compared by, and which versions make one game.
#include "library/titles.hpp"
#include "library/version_defaults.hpp"
#include "support/test_main.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {

using opensu::library::copiesOf;
using opensu::library::Game;
using opensu::library::gameTitles;
using opensu::library::Source;
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
    const std::vector<opensu::library::Title> titles = gameTitles(games);
    expect(titles.size() == 4, "Alpha across three stores and a ROM, Beta, and two unkeyed games");

    // Alpha: installed Steam first, then GOG, Epic and the ROM.
    expect(titles[0].copies.size() == 4, "three stores and a ROM hold Alpha");
    expect(titles[0].copies[0]->id == "steam:a" && titles[0].copies[1]->id == "gog:a" &&
               titles[0].copies[2]->id == "epic:a" && titles[0].copies[3]->id == "rom:alpha",
           "the installed copy first, then Steam, GOG, Epic, ROMs");
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
    const std::vector<opensu::library::Title> titles = gameTitles(games);
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
           "a game outside the catalog is its own copy");
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
    const std::vector<Game> versions = TitleCopies{games}.offered(games[0]);
    expect(versions.size() == 4 && versions[0].id == "own:a" && versions[1].id == "epic:a" &&
               versions[2].id == "steam:a" && versions[3].id == "gog:a",
           "every version, the installed last played first");
    expect(TitleCopies{games}.offered(games[3]).empty(), "a game of one version has none");

    const std::vector<Game> oneStore{game(Source::Steam, "steam:c", "Gamma"),
                                     game(Source::Own, "own:c", "Gamma", true)};
    expect(TitleCopies{oneStore}.offered(oneStore[0]).size() == 2,
           "a store's copy and the player's own are two versions");

    const std::vector<Game> ownLeads{game(Source::Steam, "steam:d", "Delta"),
                                     game(Source::Epic, "epic:d", "Delta"),
                                     game(Source::Own, "own:d", "Delta", true)};
    const std::vector<Game> led = TitleCopies{ownLeads}.offered(ownLeads[0]);
    expect(led.size() == 3 && led[0].id == "own:d",
           "the only installed copy tops the title and has its button");
}

void testBaseKey() {
    using opensu::library::baseKey;
    expect(baseKey("X-Men Legends II: Rise of Apocalypse") == "xmenlegendsii",
           "a colon starts a subtitle");
    expect(baseKey("X-Men Legends II - Rise of Apocalypse") == "xmenlegendsii",
           "so does a dash between spaces");
    expect(baseKey("Spider-Man") == "spiderman", "a dash inside a word does not");
}

/// The X-Men Legends II shapes, ROMs titled as the scan cleans them: two ROMs on two consoles, a
/// port and a Lutris Wine install.
std::vector<Game> xmen() {
    std::vector<Game> games{
        game(Source::Rom, "rom:ps2", "X-Men Legends II - Rise of Apocalypse"),
        game(Source::Rom, "rom:xbox", "X-Men Legends II - Rise of Apocalypse"),
        game(Source::Own, "own:port", "X-Men Legends II", true),
        game(Source::Own, "own:lutris", "X-Men Legends II: Rise of Apocalypse", true),
        game(Source::Steam, "steam:other", "Other"),
    };
    games[0].sourceId = "ps2";
    games[1].sourceId = "xbox";
    return games;
}

void testVersions() {
    const std::vector<Game> games = xmen();
    const std::vector<opensu::library::Title> titles = gameTitles(games);
    expect(titles.size() == 2 && titles[0].copies.size() == 4,
           "two ROMs, a port and a Lutris install are one game");
    expect(titles[0].copies[0]->source == Source::Own && titles[0].copies[2]->source == Source::Rom,
           "installed PC versions before ROMs");

    std::vector<Game> two = xmen();
    two.push_back(game(Source::Own, "own:other", "X-Men Legends II: Another Game", true));
    expect(gameTitles(two).size() == 4, "a base title shared by two subtitled games joins neither");

    std::vector<Game> chosen = xmen();
    opensu::library::VersionDefaults defaults;
    defaults.makeDefault("rom:ps2", {"rom:ps2", "rom:xbox", "own:port", "own:lutris"});
    defaults.apply(chosen);
    expect(TitleCopies{chosen}.top(chosen[2])->id == "rom:ps2",
           "the version the player made default leads its game");

    const std::vector<Game> remakes{game(Source::Steam, "steam:1", "Doom (2016)"),
                                    game(Source::Gog, "gog:2", "Doom")};
    expect(gameTitles(remakes).size() == 2,
           "a year is part of the title: a remake is another game");
}

Game onDesk(std::string id, std::string title, std::string source, std::string option) {
    Game out = game(Source::Remote, std::move(id), std::move(title), true);
    out.hostId = "desk";
    out.hostName = "Desk";
    out.remote.source = std::move(source);
    out.remote.option = std::move(option);
    return out;
}

void testRemoteVersions() {
    const std::vector<Game> games{
        game(Source::Rom, "rom:crash", "Crash Bandicoot", true),
        onDesk("lan:desk:rom:crash#DuckStation", "Crash Bandicoot", "rom", "DuckStation"),
        onDesk("lan:desk:rom:crash#SwanStation", "Crash Bandicoot", "rom", "SwanStation"),
    };
    const std::vector<opensu::library::Title> titles = gameTitles(games);
    expect(titles.size() == 1 && titles[0].copies.size() == 3,
           "each way another openSU launches a game is a version of it here");
    expect(titles[0].copies[0]->source == Source::Rom, "the version here leads");
    const opensu::library::TitleCopies copies{games};
    expect(copies.hostsOf(games[0]) == std::vector<std::string>{"Desk"},
           "the openSUs that hold it too, each once");
    expect(copies.hostsOf(games[1]).empty(), "a version there leaves its own host out");

    const std::vector<Game> stores{onDesk("lan:desk:steam:1", "Doom", "steam", ""),
                                   onDesk("lan:desk:steam:2", "DOOM", "steam", "")};
    expect(gameTitles(stores).size() == 2, "one store there holds one version of a game");

    std::vector<Game> away = games;
    away[1].hostOnline = false;
    away[2].hostOnline = false;
    const std::vector<opensu::library::Title> here = gameTitles(away);
    expect(here.size() == 1 && here[0].copies.size() == 1,
           "an openSU that is offline holds no version of a game");
    expect(gameTitles({away[1]}).empty(), "its games alone are no game here");
}

} // namespace

void testSteamApps() {
    const auto own = [](std::string id, std::string title, std::string app) {
        Game out = game(Source::Own, std::move(id), std::move(title), true);
        out.artworkKey = std::move(app);
        return out;
    };
    expect(opensu::library::steamAppOf(own("own:1", "Remake", "2050650")) == 2050650U,
           "an added game is the Steam app it is matched to");
    const std::vector<Game> games{
        game(Source::Steam, "steam:254700", "Resident Evil 4"),
        own("own:remake", "Resident Evil 4", "2050650"),
        own("own:hd", "RE4 HD Project", "254700"),
        game(Source::Rom, "rom:re4", "Resident Evil 4"),
    };
    const std::vector<opensu::library::Title> titles = gameTitles(games);
    expect(titles.size() == 2, "the 2005 game and its remake share a title but are two Steam apps");
    const auto remake = std::ranges::find_if(titles, [](const opensu::library::Title& title) {
        return title.copies.front()->id == "own:remake";
    });
    expect(remake != titles.end() && remake->copies.size() == 1 &&
               titles[remake == titles.begin() ? 1 : 0].copies.size() == 3,
           "a folder copy named otherwise joins the Steam app it is, and the ROM goes by title");
}

int main() try {
    testKey();
    testMerge();
    testSameStore();
    testCopiesOf();
    testSharedData();
    testTopFollowsPlayHistory();
    testNeverPlayedOrder();
    testOffered();
    testBaseKey();
    testVersions();
    testRemoteVersions();
    testSteamApps();
    std::printf("titles: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

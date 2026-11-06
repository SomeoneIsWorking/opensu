// Title merging across stores: the key titles are compared by, and which copies make one title.
#include "library/titles.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
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

void testUseFollowsTheRule() {
    const std::vector<Game> games = owned();
    const opensu::library::CopyChoices none;
    const TitleCopies plain{games, none};
    expect(plain.use(games[0], std::nullopt)->id == "epic:a", "Home takes the preferred copy");
    expect(plain.use(games[2], Source::Steam)->id == "steam:a", "a store's page takes its own");
    expect(plain.use(games[3], Source::Steam)->id == "gog:b",
           "a page of a store without the title takes the preferred copy");

    opensu::library::CopyChoices chosen;
    chosen.choose(games[1]);
    const TitleCopies picked{games, chosen};
    expect(picked.use(games[0], std::nullopt)->id == "gog:a" &&
               picked.use(games[0], Source::Steam)->id == "gog:a",
           "the player's choice wins everywhere, not installed or not");
    expect(picked.use(games[3], std::nullopt)->id == "gog:b", "other titles are not touched");

    const std::vector<Game> gone{games[0], games[2]};
    const TitleCopies lost{gone, chosen};
    expect(lost.use(gone[0], std::nullopt)->id == "epic:a" &&
               lost.use(gone[1], Source::Steam)->id == "steam:a",
           "a store that stopped owning the title falls back to the rule");
    expect(chosen.chosen(games[0]) == Source::Gog, "and the choice stays for its return");

    const Game stranger = game(Source::Rom, "rom:x", "Alpha");
    expect(picked.use(stranger, std::nullopt) == nullptr, "a game of no title has no copy");
}

void testPagesThatKeepTheirCopy() {
    std::vector<Game> games = owned();
    games.push_back(game(Source::Own, "own:a", "Alpha"));
    opensu::library::CopyChoices chosen;
    chosen.choose(games[1]);
    const TitleCopies copies{games, chosen};
    expect(copies.use(games[4], Source::Own)->id == "own:a",
           "the player's own page keeps its copy past a store's choice");
    expect(copies.switchable(games[4], Source::Own).empty(), "and offers no switch");
    expect(copies.switchable(games[4], std::nullopt).size() == 3,
           "only the stores are switched between");
}

void testInstallable() {
    const std::vector<Game> games = owned();
    const opensu::library::CopyChoices none;
    const TitleCopies plain{games, none};
    expect(plain.installable(games[0], std::nullopt).size() == 3, "Home offers every copy");
    expect(plain.installable(games[0], Source::Steam).size() == 1 &&
               plain.installable(games[0], Source::Steam)[0].id == "steam:a",
           "a store's page installs its own copy");

    opensu::library::CopyChoices chosen;
    chosen.choose(games[1]);
    const TitleCopies picked{games, chosen};
    expect(picked.installable(games[1], std::nullopt).size() == 1 &&
               picked.installable(games[1], std::nullopt)[0].id == "gog:a",
           "a chosen copy installs alone on Home");
    expect(picked.installable(games[1], Source::Steam)[0].id == "gog:a",
           "and on a page of another store");
    const Game stranger = game(Source::Rom, "rom:x", "Alpha");
    expect(picked.installable(stranger, std::nullopt).size() == 1, "a stranger offers itself");
}

void testSwitchable() {
    const std::vector<Game> games = owned();
    const opensu::library::CopyChoices none;
    const TitleCopies copies{games, none};
    const std::vector<Game> stores = copies.switchable(games[2], std::nullopt);
    expect(stores.size() == 3 && stores[0].source == Source::Steam &&
               stores[1].source == Source::Gog && stores[2].source == Source::Epic,
           "the stores owning the title, Steam, GOG, Epic");
    expect(copies.switchable(games[3], std::nullopt).empty(), "one store has nothing to switch");
}

} // namespace

int main() try {
    testKey();
    testMerge();
    testSameStore();
    testCopiesOf();
    testUseFollowsTheRule();
    testPagesThatKeepTheirCopy();
    testInstallable();
    testSwitchable();
    std::printf("titles: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

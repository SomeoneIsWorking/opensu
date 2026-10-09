// Title merging across stores: the key titles are compared by, and which copies make one title.
#include "library/titles.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::library::copiesOf;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::storeTitles;
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

} // namespace

int main() {
    testKey();
    testMerge();
    testSameStore();
    testCopiesOf();
    std::printf("titles: all checks passed\n");
    return 0;
}

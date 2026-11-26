// Shelves: Home's installed games, Library's launchers, consoles and Add tile, a console's ROMs, a
// store's library and every game once, going in and back out and between sections; and the
// catalog's per-store status that the launcher badges read.
#include "library/shelf.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "library/catalog_loader.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::library::AddTile;
using opensu::library::Availability;
using opensu::library::Catalog;
using opensu::library::Console;
using opensu::library::Game;
using opensu::library::Giveaways;
using opensu::library::HomeChoices;
using opensu::library::Launcher;
using opensu::library::Section;
using opensu::library::ShelfBrowser;
using opensu::library::ShelfItem;
using opensu::library::Source;
using opensu::library::SourceStatus;

std::size_t cycleFrom(ShelfBrowser& browser, int delta, std::size_t focus) {
    browser.leave(focus);
    return browser.cycle(delta);
}

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game game(Source source, std::string id, std::string sourceId = {}, std::string title = {},
          bool installed = true) {
    Game out;
    out.source = source;
    out.id = std::move(id);
    out.title = title.empty() ? out.id : std::move(title);
    out.sourceId = std::move(sourceId);
    out.installed = installed;
    return out;
}

/// Hades is owned in all three stores and installed from Steam; Celeste in Steam (not installed)
/// and Epic (installed); Tunic in GOG only, not installed; one game is Steam's alone.
std::vector<Game> library() {
    return {game(Source::Steam, "steam:hades", "1", "Hades"),
            game(Source::Rom, "rom:ps2-a", "ps2"),
            game(Source::Rom, "rom:gc-a", "gc"),
            game(Source::Epic, "epic:celeste", "celeste", "Celeste", true),
            game(Source::Steam, "steam:solo", "3", "Only Steam"),
            game(Source::Rom, "rom:ps2-b", "ps2"),
            game(Source::Epic, "epic:hades", "hades", "HADES™", false),
            game(Source::Steam, "steam:celeste", "2", "Celeste", false),
            game(Source::Gog, "gog:hades", "hades", "Hades", false),
            game(Source::Gog, "gog:tunic", "tunic", "Tunic", false)};
}

std::vector<SourceStatus> allReady() {
    return {{Source::Steam, Availability::Ready, {}},
            {Source::Epic, Availability::Ready, {}},
            {Source::Gog, Availability::Ready, {}}};
}

const Console* consoleAt(const std::vector<ShelfItem>& shelf, std::size_t index) {
    return std::get_if<Console>(&shelf.at(index));
}

const Game* gameAt(const std::vector<ShelfItem>& shelf, std::size_t index) {
    return std::get_if<Game>(&shelf.at(index));
}

const Launcher* launcherAt(const std::vector<ShelfItem>& shelf, std::size_t index) {
    return std::get_if<Launcher>(&shelf.at(index));
}

std::string keyOf(const std::optional<opensu::library::Folder>& folder) {
    return folder ? opensu::library::key(*folder) : std::string{};
}

std::vector<Source> stores(std::initializer_list<Source> list) {
    return list;
}

void testHomeShelf() {
    const std::vector<ShelfItem> home = opensu::library::homeShelf(library(), {}, {}, {});
    // GameCube, PS2, then Hades (Steam), Celeste (Epic), Only Steam: the installed titles once.
    expect(home.size() == 5, "Home holds the consoles and the installed titles");
    expect(*consoleAt(home, 0) == Console{"gc", "GameCube", 1} &&
               *consoleAt(home, 1) == Console{"ps2", "PlayStation 2", 2},
           "every console with a ROM comes first, in Library's order");
    expect(gameAt(home, 2)->id == "steam:hades" && gameAt(home, 3)->id == "epic:celeste" &&
               gameAt(home, 4)->id == "steam:solo",
           "Home keeps each installed title as the copy installed here");
    expect(gameAt(home, 2)->ownedIn == stores({Source::Steam, Source::Gog, Source::Epic}),
           "an installed title lists every store that owns it");
    expect(std::ranges::none_of(home,
                                [](const ShelfItem& item) {
                                    const Game* rom = std::get_if<Game>(&item);
                                    return rom != nullptr && rom->source == Source::Rom;
                                }),
           "no ROM and no other folder is on Home");
    expect(opensu::library::homeShelf({}, {}, {}, {}).empty(),
           "nothing installed is an empty Home");
}

void testVersionsOnHome() {
    std::vector<Game> games{
        game(Source::Rom, "rom:ps2-x", "ps2", "X-Men Legends II - Rise of Apocalypse", false),
        game(Source::Own, "own:port", {}, "X-Men Legends II"),
    };
    HomeChoices choices;
    std::vector<ShelfItem> home = opensu::library::homeShelf(games, {}, {}, choices);
    expect(home.size() == 2 && gameAt(home, 1) != nullptr && gameAt(home, 1)->id == "own:port",
           "a game with a ROM is on Home once, as its installed PC version");
    opensu::library::setOnHome(games, games[0], false, choices);
    home = opensu::library::homeShelf(games, {}, {}, choices);
    expect(home.size() == 1, "taken off Home through its ROM, the game stays off");
}

Game onDesk(std::string id, std::string title, std::optional<std::size_t> home) {
    Game out = game(Source::Remote, std::move(id), {}, std::move(title));
    out.hostId = "desk";
    out.hostName = "Desk";
    out.remote.source = "steam";
    out.remote.home = home;
    return out;
}

void testPairedHomes() {
    const std::vector<Game> games{
        game(Source::Steam, "steam:hades", {}, "Hades"),
        onDesk("lan:desk:steam:2", "Tunic", 1),
        onDesk("lan:desk:steam:1", "Hades", 0),
        onDesk("lan:desk:steam:3", "Celeste", 2),
        onDesk("lan:desk:steam:4", "Off Home", std::nullopt),
    };
    const std::vector<SourceStatus> lan{{Source::Remote, Availability::Ready, {}}};
    const std::vector<ShelfItem> home = opensu::library::homeShelf(games, lan, {}, HomeChoices{});
    expect(home.size() == 4 && gameAt(home, 0)->id == "steam:hades",
           "this Home first, then Desk's");
    expect(launcherAt(home, 1) != nullptr && launcherAt(home, 1)->hostName == "Desk",
           "a paired openSU's Home opens with its launcher");
    expect(gameAt(home, 2)->title == "Tunic" && gameAt(home, 3)->title == "Celeste",
           "its games in its order, leaving out one shown already and one not on its Home");
    expect(opensu::library::homeShelf(games, {}, {}, HomeChoices{}).size() == 1,
           "with LAN off, no paired Home");
    expect(opensu::library::homeGameIds(games, HomeChoices{}) ==
               std::vector<std::string>{"steam:hades"},
           "another openSU is told this Home's own versions");
}

void testPlayingOnHost() {
    std::vector<Game> games{onDesk("lan:desk:steam:1", "Hades", 0),
                            onDesk("lan:desk:steam:2", "Tunic", 1)};
    expect(opensu::library::playingOn(games, "desk") == nullptr, "Desk plays nothing");
    games[1].playingAs = {"ana"};
    const Game* playing = opensu::library::playingOn(games, "desk");
    expect(playing != nullptr && playing->title == "Tunic", "Desk plays Tunic");
    expect(opensu::library::playingOn(games, "room") == nullptr, "another openSU plays nothing");
    const std::vector<SourceStatus> lan{{Source::Remote, Availability::Ready, {}}};
    const std::vector<ShelfItem> home = opensu::library::homeShelf(games, lan, {}, HomeChoices{});
    expect(launcherAt(home, 0) != nullptr && launcherAt(home, 0)->playing == "Tunic",
           "Desk's launcher says what plays there");
}

void testHomeChoices() {
    const std::vector<Game> games = library();
    HomeChoices choices;
    opensu::library::setOnHome(games, games.at(2), true, choices);
    opensu::library::setOnHome(games, Console{"gc", "GameCube", 1}, false, choices);
    opensu::library::setOnHome(games, games.at(9), true, choices);
    opensu::library::setOnHome(games, games.at(4), false, choices);
    const std::vector<ShelfItem> home = opensu::library::homeShelf(games, {}, {}, choices);
    // PS2, then Hades, the GC ROM, Celeste, Tunic in the games' order; GameCube and Only Steam
    // taken off.
    expect(home.size() == 5 && consoleAt(home, 0) != nullptr &&
               consoleAt(home, 0)->system == "ps2" && gameAt(home, 1)->id == "steam:hades" &&
               gameAt(home, 2)->id == "rom:gc-a" && gameAt(home, 3)->id == "epic:celeste" &&
               gameAt(home, 4)->id == "gog:tunic",
           "consoles not taken off first, then added and installed games in the games' order");
    expect(opensu::library::onHome(games, Console{"gc", "GameCube", 1}, choices) == false &&
               opensu::library::onHome(games, Console{"ps2", "PlayStation 2", 2}, choices) == true,
           "a console taken off is off Home, one never touched is on it");
    expect(opensu::library::onHome(games, games.at(6), choices) == true,
           "a title's other copy is on Home with the title");
    expect(opensu::library::onHome(games, games.at(4), choices) == false &&
               opensu::library::onHome(games, games.at(5), choices) == false &&
               opensu::library::onHome(games, Giveaways{}, choices) == std::nullopt,
           "a removed game, a ROM not added; Giveaways cannot be put on Home");
    opensu::library::setOnHome(games, games.at(4), true, choices);
    expect(choices.entries().size() == 3,
           "putting an installed game back forgets the choice, so it follows the default again");
    opensu::library::setOnHome(games, games.at(7), false, choices);
    expect(opensu::library::onHome(games, games.at(3), choices) == false,
           "taking a title's other copy off takes the title off");
}

/// The add tiles the shell passes: Add a game alone.
std::vector<AddTile> oneAdd() {
    return {AddTile{"add", "Add", "Stores, consoles, games"}};
}

void testLibraryOnHome() {
    const std::vector<Game> games = library();
    HomeChoices choices;
    const Launcher steam{Source::Steam, 3, true};
    expect(opensu::library::onHome(games, steam, choices) == false,
           "a store's library is off Home until the player adds it");
    opensu::library::setOnHome(games, steam, true, choices);
    const std::vector<ShelfItem> home = opensu::library::homeShelf(games, allReady(), {}, choices);
    expect(launcherAt(home, 0) != nullptr && launcherAt(home, 0)->source == Source::Steam &&
               consoleAt(home, 1) != nullptr,
           "added, Steam's library stands first, in Library's order before the consoles");
    expect(opensu::library::onHome(games, steam, choices) == true, "and reads as on Home");
}

void testEmptyHome() {
    const std::vector<SourceStatus> listed{{Source::Rom, Availability::Ready, {}}};
    const std::vector<ShelfItem> empty = opensu::library::homeShelf({}, listed, oneAdd(), {});
    expect(empty.size() == 1 && std::get<AddTile>(empty.at(0)) == oneAdd().at(0),
           "a Home with nothing on it holds the add tiles");
    const std::vector<SourceStatus> loading{{Source::Steam, Availability::Loading, {}}};
    expect(opensu::library::homeShelf({}, loading, oneAdd(), {}).empty(),
           "but not while a store's first listing is still out");
    const std::vector<ShelfItem> home = opensu::library::homeShelf(library(), listed, oneAdd(), {});
    expect(home.size() == 5 &&
               std::ranges::none_of(home,
                                    [](const ShelfItem& item) {
                                        return std::holds_alternative<AddTile>(item);
                                    }),
           "a Home with a game leaves the add tiles to Library");
    const std::vector<Game> roms{game(Source::Rom, "rom:gc-a", "gc")};
    const std::vector<ShelfItem> romsOnly = opensu::library::homeShelf(roms, listed, oneAdd(), {});
    expect(romsOnly.size() == 1 && *consoleAt(romsOnly, 0) == Console{"gc", "GameCube", 1},
           "a folder of ROMs alone puts its console on Home in place of the add tiles");
    HomeChoices removed;
    opensu::library::setOnHome(roms, Console{"gc", "GameCube", 1}, false, removed);
    const std::vector<ShelfItem> bare = opensu::library::homeShelf(roms, listed, oneAdd(), removed);
    expect(bare.size() == 1 && std::holds_alternative<AddTile>(bare.at(0)),
           "with that console taken off, Home holds the add tiles again");
}

void testLibraryShelf() {
    const std::vector<ShelfItem> shelf =
        opensu::library::libraryShelf(library(), allReady(), oneAdd());
    // Three launchers, Giveaways, two consoles, Add.
    expect(shelf.size() == 7, "launchers, Giveaways, consoles and Add");
    expect(*launcherAt(shelf, 0) == Launcher{Source::Steam, 3, true}, "Steam counts its games");
    expect(*launcherAt(shelf, 1) == Launcher{Source::Epic, 2, true}, "Epic follows Steam");
    expect(*launcherAt(shelf, 2) == Launcher{Source::Gog, 2, true}, "GOG follows Epic");
    expect(std::holds_alternative<Giveaways>(shelf.at(3)), "Giveaways follows the stores");
    expect(*consoleAt(shelf, 4) == Console{"gc", "GameCube", 1}, "GameCube first, in system order");
    expect(*consoleAt(shelf, 5) == Console{"ps2", "PlayStation 2", 2}, "PS2 counts both ROMs");
    expect(std::holds_alternative<AddTile>(shelf.at(6)), "Add is last");
    expect(std::ranges::none_of(shelf,
                                [](const ShelfItem& item) {
                                    return std::holds_alternative<Game>(item);
                                }),
           "Library holds no game of its own");
}

void testLibraryLaunchers() {
    std::vector<SourceStatus> sources = allReady();
    sources[1].availability = Availability::Absent;
    sources[2].availability = Availability::Attention;
    const std::vector<ShelfItem> shelf =
        opensu::library::libraryShelf(library(), sources, oneAdd());
    expect(launcherAt(shelf, 0) != nullptr && launcherAt(shelf, 0)->source == Source::Steam &&
               launcherAt(shelf, 1) != nullptr && launcherAt(shelf, 1)->source == Source::Gog,
           "a store that is not there has no tile");
    expect(!launcherAt(shelf, 1)->ready, "a store that needs attention is not ready");

    const std::vector<ShelfItem> bare = opensu::library::libraryShelf({}, {}, oneAdd());
    expect(bare.size() == 1 && std::holds_alternative<AddTile>(bare.at(0)),
           "nothing at all is Add alone");
    const std::vector<AddTile> twoAdds{AddTile{"add-game", "Add a game", ""},
                                       AddTile{"add", "Add", ""}};
    const std::vector<ShelfItem> added = opensu::library::libraryShelf({}, {}, twoAdds);
    expect(added.size() == 2 && std::get<AddTile>(added.at(0)) == twoAdds[0] &&
               std::get<AddTile>(added.at(1)) == twoAdds[1],
           "the add tiles stand in the order the shell gives them");

    const std::vector<ShelfItem> romsOnly =
        opensu::library::libraryShelf({game(Source::Rom, "rom:gc-a", "gc")},
                                      {{Source::Steam, Availability::Absent, {}}}, oneAdd());
    expect(romsOnly.size() == 2 && consoleAt(romsOnly, 0) != nullptr &&
               std::holds_alternative<AddTile>(romsOnly.at(1)),
           "no stores, no launchers and no Giveaways");

    const std::vector<ShelfItem> signedOut = opensu::library::libraryShelf(
        {}, {{Source::Gog, Availability::Attention, "not signed in"}}, oneAdd());
    expect(signedOut.size() == 3 && !launcherAt(signedOut, 0)->ready &&
               std::holds_alternative<Giveaways>(signedOut.at(1)),
           "a signed-out store still has its tile and Giveaways");

    const std::vector<ShelfItem> ownOnly =
        opensu::library::libraryShelf({game(Source::Own, "own:1", "1", "Celeste", true)},
                                      {{Source::Own, Availability::Ready, {}}}, oneAdd());
    expect(ownOnly.size() == 2 && *launcherAt(ownOnly, 0) == Launcher{Source::Own, 1, true} &&
               std::holds_alternative<AddTile>(ownOnly.at(1)),
           "the player's own games are a launcher without Giveaways");
    expect(
        opensu::library::homeShelf({game(Source::Own, "own:1", "1", "Celeste", true)}, {}, {}, {})
                .size() == 1,
        "an added game is on Home");
}

void testConsoleShelf() {
    const std::vector<ShelfItem> ps2 = opensu::library::consoleShelf(library(), "ps2");
    expect(ps2.size() == 2 && gameAt(ps2, 0)->id == "rom:ps2-a" &&
               gameAt(ps2, 1)->id == "rom:ps2-b",
           "a console holds its own ROMs in catalog order");
    expect(gameAt(ps2, 0)->ownedIn.empty(), "a ROM shows no stores");
    expect(opensu::library::consoleShelf(library(), "n64").empty(), "no ROMs, empty shelf");
}

void testLauncherShelf() {
    const std::vector<ShelfItem> steam =
        opensu::library::launcherShelf(library(), Launcher{Source::Steam});
    expect(steam.size() == 3 && gameAt(steam, 0)->id == "steam:hades" &&
               gameAt(steam, 1)->id == "steam:solo" && gameAt(steam, 2)->id == "steam:celeste",
           "a store holds its whole library, installed or not, in catalog order");
    expect(gameAt(steam, 0)->ownedIn.empty(), "a launcher's tile carries no store icons");
    expect(!gameAt(steam, 2)->installed,
           "the store's own copy is shown even where another store has it installed");

    const std::vector<ShelfItem> gog =
        opensu::library::launcherShelf(library(), Launcher{Source::Gog});
    expect(gog.size() == 2 && gameAt(gog, 0)->id == "gog:hades", "GOG holds its own Hades");
    expect(opensu::library::launcherShelf({}, Launcher{Source::Epic}).empty(),
           "no games, empty shelf");
}

void testEveryGame() {
    const std::vector<ShelfItem> every = opensu::library::everyGameShelf(library());
    std::vector<ShelfItem> all;
    std::ranges::copy_if(every, std::back_inserter(all), [](const ShelfItem& item) {
        return std::get<Game>(item).source != Source::Rom;
    });
    expect(every.size() == 7 && all.size() == 4, "four titles across the stores, and three ROMs");
    expect(gameAt(all, 0)->id == "steam:hades" &&
               gameAt(all, 0)->ownedIn == stores({Source::Steam, Source::Gog, Source::Epic}),
           "a title in every store is one entry from the preferred store");
    expect(gameAt(all, 1)->id == "epic:celeste" && gameAt(all, 1)->installed &&
               gameAt(all, 1)->ownedIn == stores({Source::Steam, Source::Epic}),
           "an installed copy wins over a preferred store");
    expect(gameAt(all, 2)->id == "steam:solo" && gameAt(all, 2)->ownedIn == stores({Source::Steam}),
           "a title in one store lists that store");
    expect(gameAt(all, 3)->id == "gog:tunic", "uninstalled titles are there too");
}

void testBrowser() {
    ShelfBrowser browser;
    expect(browser.section() == Section::Home, "the browser starts on Home");
    expect(!browser.back().has_value(), "back at a section's top goes nowhere");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 5, "Home's shelf");

    expect(cycleFrom(browser, 1, 2) == 0 && browser.section() == Section::Library,
           "R1 reaches Library, first visit at its first slot");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 7, "Library's shelf");
    browser.open(Console{"ps2", "PlayStation 2", 2}, 5);
    expect(keyOf(browser.folder()) == "ps2", "the console is open");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 2, "its shelf is its ROMs");
    expect(browser.back() == 5, "back restores Library's focus on the console");
    expect(!browser.folder() && browser.section() == Section::Library &&
               browser.shelf(library(), allReady(), oneAdd(), {}).size() == 7,
           "Library again, not Home");

    browser.open(Console{"ps2", "PlayStation 2", 2}, 5);
    std::vector<Game> gone = library();
    std::erase_if(gone, [](const Game& entry) {
        return entry.sourceId == "ps2";
    });
    expect(browser.shelf(gone, allReady(), oneAdd(), {}).size() == 6 && !browser.folder(),
           "a console whose ROMs are gone returns to Library");

    browser.open(Launcher{Source::Epic, 2, true}, 1);
    expect(browser.launcher() == Source::Epic && keyOf(browser.folder()) == "launcher:epic",
           "a launcher is open");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 2,
           "its shelf is its store's games");
    expect(browser.back() == 1, "back restores the launcher's slot");
}

void testCycling() {
    ShelfBrowser browser;
    expect(cycleFrom(browser, 1, 2) == 0 && browser.section() == Section::Library,
           "Home to Library");
    expect(cycleFrom(browser, 1, 4) == 2 && browser.section() == Section::Home,
           "Library wraps to Home, which is where it was left");
    expect(cycleFrom(browser, -1, 2) == 4 && browser.section() == Section::Library,
           "L1 from Home wraps to Library, which is where it was left");

    browser.open(Console{"gc", "GameCube", 1}, 4);
    expect(cycleFrom(browser, 1, 0) == 2 && !browser.folder() && browser.section() == Section::Home,
           "leaving Library closes the folder");
    expect(cycleFrom(browser, 1, 2) == 4,
           "Library is remembered at the slot the folder was entered from, not the folder's");
}

void testFolders() {
    expect(!opensu::library::folderOf(ShelfItem{game(Source::Steam, "steam:1")}),
           "a game opens nothing");
    const std::optional<opensu::library::Folder> console =
        opensu::library::folderOf(ShelfItem{Console{"gc", "GameCube", 1}});
    expect(console && opensu::library::name(*console) == "GameCube", "a console's name");
    expect(opensu::library::name(Launcher{Source::Gog, 0, false}) == "GOG",
           "a launcher is its store");
}

class FakeProvider final : public opensu::library::Provider {
  public:
    enum class Mode : std::uint8_t { Lists, Absent, Fails };
    FakeProvider(Source source, Mode mode) : source_{source}, mode_{mode} {
    }
    [[nodiscard]] Source source() const override {
        return source_;
    }
    [[nodiscard]] std::vector<Game> list(const std::stop_token& /*stop*/) override {
        if (mode_ == Mode::Absent) {
            throw opensu::library::SourceAbsent{"not installed"};
        }
        if (mode_ == Mode::Fails) {
            throw std::runtime_error{"not logged in"};
        }
        return {game(source_, "steam:9", "9")};
    }

  private:
    Source source_;
    Mode mode_;
};

void testLoadingLauncher() {
    const std::vector<SourceStatus> sources{{Source::Steam, Availability::Ready, {}},
                                            {Source::Epic, Availability::Loading, {}}};
    const std::vector<ShelfItem> shelf = opensu::library::libraryShelf({}, sources, oneAdd());
    const Launcher* epic = launcherAt(shelf, 1);
    expect(epic != nullptr && epic->loading && !epic->ready,
           "a store whose listing is not in is loading, and is not asking to sign in");
    expect(!launcherAt(shelf, 0)->loading, "a store that listed is not loading");
}

void testCatalogStatuses() {
    Catalog catalog;
    catalog.add(std::make_unique<FakeProvider>(Source::Steam, FakeProvider::Mode::Lists));
    catalog.add(std::make_unique<FakeProvider>(Source::Epic, FakeProvider::Mode::Fails));
    catalog.add(std::make_unique<FakeProvider>(Source::Gog, FakeProvider::Mode::Absent));
    opensu::library::CatalogLoader loader{std::move(catalog)};
    loader.refresh();
    loader.waitIdle();
    const std::optional<opensu::library::CatalogSnapshot> taken = loader.take();
    if (!taken) {
        expect(false, "the listings arrive");
        return;
    }
    const opensu::library::CatalogSnapshot& snapshot = *taken;
    expect(snapshot.games.size() == 1, "the working store still contributes");
    expect(snapshot.sources.size() == 3, "every store reports");
    expect(snapshot.sources[0].availability == Availability::Ready, "a store that lists is ready");
    expect(snapshot.sources[1].availability == Availability::Attention &&
               snapshot.sources[1].detail == "not logged in",
           "a store that fails needs attention, with the reason");
    expect(snapshot.sources[2].availability == Availability::Absent,
           "a store not installed is absent, not a fault");
}

} // namespace

void testRemoteHosts() {
    const auto remote = [](std::string host, std::string name, const std::string& id) {
        Game out = game(Source::Remote, "lan:" + host + ":" + id, id, id);
        out.hostId = std::move(host);
        out.hostName = std::move(name);
        return out;
    };
    const std::vector<Game> games{game(Source::Own, "own:1", "1", "Celeste", true),
                                  remote("bb", "Living room", "a"), remote("aa", "Desk", "b"),
                                  remote("bb", "Living room", "c")};
    const std::vector<ShelfItem> shelf = opensu::library::libraryShelf(
        games, {{Source::Own, Availability::Ready, {}}, {Source::Remote, Availability::Ready, {}}},
        oneAdd());
    const Launcher* desk = launcherAt(shelf, 1);
    const Launcher* room = launcherAt(shelf, 2);
    expect(launcherAt(shelf, 0)->source == Source::Own && desk != nullptr && room != nullptr &&
               desk->hostName == "Desk" && desk->games == 1 && room->hostName == "Living room" &&
               room->games == 2,
           "each openSU on the LAN is a launcher after the player's own, by its name");
    expect(keyOf(opensu::library::folderOf(shelf.at(2))) == "launcher:lan:bb",
           "named by its peer id");
    const std::vector<ShelfItem> held = opensu::library::launcherShelf(games, *room);
    expect(held.size() == 2 && gameAt(held, 0)->id == "lan:bb:a" &&
               gameAt(held, 1)->id == "lan:bb:c",
           "it holds that openSU's games only");
    expect(opensu::library::homeShelf(games, {}, {}, {}).size() == 1,
           "Home leaves them to their launchers");

    std::vector<Game> away = games;
    away[2].hostOnline = false;
    const std::vector<ShelfItem> offline = opensu::library::libraryShelf(
        away, {{Source::Own, Availability::Ready, {}}, {Source::Remote, Availability::Ready, {}}},
        oneAdd());
    const Launcher* gone = launcherAt(offline, 1);
    expect(gone != nullptr && gone->hostName == "Desk" && !gone->online && gone->games == 1 &&
               opensu::library::launcherShelf(away, *gone).size() == 1,
           "an offline openSU keeps its launcher, which still lists its games");
    const std::vector<ShelfItem> every = opensu::library::everyGameShelf(away);
    expect(std::ranges::none_of(every,
                                [](const ShelfItem& item) {
                                    const Game* found = std::get_if<Game>(&item);
                                    return found != nullptr && found->hostId == "aa";
                                }),
           "nowhere else lists its games");
}

int main() try {
    try {
        testHomeShelf();
        testHomeChoices();
        testLibraryOnHome();
        testVersionsOnHome();
        testPairedHomes();
        testEmptyHome();
        testLibraryShelf();
        testLibraryLaunchers();
        testConsoleShelf();
        testLauncherShelf();
        testRemoteHosts();
        testPlayingOnHost();
        testEveryGame();
        testBrowser();
        testCycling();
        testFolders();
        testLoadingLauncher();
        testCatalogStatuses();
        std::printf("shelf: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}

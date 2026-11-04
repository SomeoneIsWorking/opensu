// Shelves: Home's installed games, Library's launchers and consoles, a console's ROMs, a store's
// library and the combined library, going in and back out and between sections; and the catalog's
// per-store status that the launcher badges read.
#include "library/shelf.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
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
using opensu::library::AllGames;
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
    const std::vector<ShelfItem> home = opensu::library::homeShelf(library(), {});
    // Hades (Steam), Celeste (Epic), Only Steam: the installed titles, each once.
    expect(home.size() == 3, "Home holds only the installed titles");
    expect(gameAt(home, 0)->id == "steam:hades" && gameAt(home, 1)->id == "epic:celeste" &&
               gameAt(home, 2)->id == "steam:solo",
           "Home keeps each installed title as the copy installed here");
    expect(gameAt(home, 0)->ownedIn == stores({Source::Steam, Source::Gog, Source::Epic}),
           "an installed title lists every store that owns it");
    expect(std::ranges::none_of(home,
                                [](const ShelfItem& item) {
                                    return std::get<Game>(item).source == Source::Rom;
                                }),
           "no ROM and no folder is on Home");
    expect(opensu::library::homeShelf({}, {}).empty(), "nothing installed is an empty Home");
}

void testHomeChoices() {
    const std::vector<Game> games = library();
    HomeChoices choices;
    opensu::library::setOnHome(games, games.at(2), true, choices);
    opensu::library::setOnHome(games, Console{"ps2", "PlayStation 2", 2}, true, choices);
    opensu::library::setOnHome(games, games.at(9), true, choices);
    opensu::library::setOnHome(games, games.at(4), false, choices);
    const std::vector<ShelfItem> home = opensu::library::homeShelf(games, choices);
    // PS2, then Hades, the GC ROM, Celeste, Tunic in the games' order; Only Steam taken off.
    expect(home.size() == 5 && consoleAt(home, 0) != nullptr &&
               consoleAt(home, 0)->system == "ps2" && gameAt(home, 1)->id == "steam:hades" &&
               gameAt(home, 2)->id == "rom:gc-a" && gameAt(home, 3)->id == "epic:celeste" &&
               gameAt(home, 4)->id == "gog:tunic",
           "added consoles first, then added and installed games in the games' order");
    expect(opensu::library::onHome(games, games.at(6), choices) == true,
           "a title's other copy is on Home with the title");
    expect(opensu::library::onHome(games, games.at(4), choices) == false &&
               opensu::library::onHome(games, games.at(5), choices) == false &&
               opensu::library::onHome(games, AllGames{}, choices) == std::nullopt,
           "a removed game, a ROM not added; a folder cannot be put on Home");
    opensu::library::setOnHome(games, games.at(4), true, choices);
    expect(choices.entries().size() == 3,
           "putting an installed game back forgets the choice, so it follows the default again");
    opensu::library::setOnHome(games, games.at(7), false, choices);
    expect(opensu::library::onHome(games, games.at(3), choices) == false,
           "taking a title's other copy off takes the title off");
}

/// The add tiles the shell passes: Add a game alone.
std::vector<AddTile> oneAdd() {
    return {AddTile{"add-game", "Add a game", "A file"}};
}

void testLibraryShelf() {
    const std::vector<ShelfItem> shelf =
        opensu::library::libraryShelf(library(), allReady(), oneAdd());
    // Three launchers, Giveaways, Add a game, All games, two consoles.
    expect(shelf.size() == 8, "launchers, Giveaways, Add a game, All games and consoles");
    expect(*launcherAt(shelf, 0) == Launcher{Source::Steam, 3, true}, "Steam counts its games");
    expect(*launcherAt(shelf, 1) == Launcher{Source::Epic, 2, true}, "Epic follows Steam");
    expect(*launcherAt(shelf, 2) == Launcher{Source::Gog, 2, true}, "GOG follows Epic");
    expect(std::holds_alternative<Giveaways>(shelf.at(3)), "Giveaways follows the stores");
    expect(std::holds_alternative<AddTile>(shelf.at(4)), "Add a game follows Giveaways");
    expect(std::get_if<AllGames>(&shelf.at(5)) != nullptr &&
               std::get<AllGames>(shelf.at(5)).games == 4,
           "All games counts titles, a title owned in three stores once");
    expect(*consoleAt(shelf, 6) == Console{"gc", "GameCube", 1}, "GameCube first, in system order");
    expect(*consoleAt(shelf, 7) == Console{"ps2", "PlayStation 2", 2}, "PS2 counts both ROMs");
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
    expect(std::get_if<AllGames>(&shelf.at(4)) != nullptr, "All games follows Add a game");

    const std::vector<ShelfItem> bare = opensu::library::libraryShelf({}, {}, oneAdd());
    expect(bare.size() == 1 && std::holds_alternative<AddTile>(bare.at(0)),
           "nothing at all is Add a game alone");
    const std::vector<AddTile> twoAdds{AddTile{"add-game", "Add a game", ""},
                                       AddTile{"set-up:steam", "Set up Steam", ""}};
    const std::vector<ShelfItem> added = opensu::library::libraryShelf({}, {}, twoAdds);
    expect(added.size() == 2 && std::get<AddTile>(added.at(0)) == twoAdds[0] &&
               std::get<AddTile>(added.at(1)) == twoAdds[1],
           "the add tiles stand in the order the shell gives them");

    const std::vector<ShelfItem> romsOnly =
        opensu::library::libraryShelf({game(Source::Rom, "rom:gc-a", "gc")},
                                      {{Source::Steam, Availability::Absent, {}}}, oneAdd());
    expect(romsOnly.size() == 2 && std::holds_alternative<AddTile>(romsOnly.at(0)) &&
               consoleAt(romsOnly, 1) != nullptr,
           "no stores, no launchers, no Giveaways and no All games");

    const std::vector<ShelfItem> signedOut = opensu::library::libraryShelf(
        {}, {{Source::Gog, Availability::Attention, "not signed in"}}, oneAdd());
    expect(signedOut.size() == 3 && !launcherAt(signedOut, 0)->ready &&
               std::holds_alternative<Giveaways>(signedOut.at(1)),
           "a signed-out store still has its tile and Giveaways, and no All games without games");

    const std::vector<ShelfItem> ownOnly =
        opensu::library::libraryShelf({game(Source::Own, "own:1", "1", "Celeste", true)},
                                      {{Source::Own, Availability::Ready, {}}}, oneAdd());
    expect(ownOnly.size() == 3 && *launcherAt(ownOnly, 0) == Launcher{Source::Own, 1, true} &&
               std::holds_alternative<AddTile>(ownOnly.at(1)) &&
               std::get<AllGames>(ownOnly.at(2)).games == 1,
           "the player's own games are a launcher without Giveaways, and in All games");
    expect(
        opensu::library::homeShelf({game(Source::Own, "own:1", "1", "Celeste", true)}, {}).size() ==
            1,
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
    expect(gameAt(steam, 0)->ownedIn == stores({Source::Gog, Source::Epic}),
           "a game owned elsewhere lists the other stores");
    expect(gameAt(steam, 1)->ownedIn.empty(), "a game owned here alone lists none");
    expect(gameAt(steam, 2)->ownedIn == stores({Source::Epic}) && !gameAt(steam, 2)->installed,
           "the store's own copy is shown even where another store has it installed");

    const std::vector<ShelfItem> gog =
        opensu::library::launcherShelf(library(), Launcher{Source::Gog});
    expect(gog.size() == 2 && gameAt(gog, 0)->ownedIn == stores({Source::Steam, Source::Epic}),
           "GOG's Hades lists Steam and Epic");
    expect(opensu::library::launcherShelf({}, Launcher{Source::Epic}).empty(),
           "no games, empty shelf");
}

void testAllGames() {
    const std::vector<ShelfItem> all = opensu::library::allGamesShelf(library());
    expect(all.size() == 4, "four titles across the stores");
    expect(gameAt(all, 0)->id == "steam:hades" &&
               gameAt(all, 0)->ownedIn == stores({Source::Steam, Source::Gog, Source::Epic}),
           "a title in every store is one entry from the preferred store");
    expect(gameAt(all, 1)->id == "epic:celeste" && gameAt(all, 1)->installed &&
               gameAt(all, 1)->ownedIn == stores({Source::Steam, Source::Epic}),
           "an installed copy wins over a preferred store");
    expect(gameAt(all, 2)->id == "steam:solo" && gameAt(all, 2)->ownedIn == stores({Source::Steam}),
           "a title in one store lists that store");
    expect(gameAt(all, 3)->id == "gog:tunic", "uninstalled titles are there too");
    expect(std::ranges::none_of(all,
                                [](const ShelfItem& item) {
                                    return std::get<Game>(item).source == Source::Rom;
                                }),
           "ROMs are not in the combined library");
}

void testBrowser() {
    ShelfBrowser browser;
    expect(browser.section() == Section::Home, "the browser starts on Home");
    expect(!browser.back().has_value(), "back at a section's top goes nowhere");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 3, "Home's shelf");

    expect(cycleFrom(browser, 1, 2) == 0 && browser.section() == Section::Library,
           "R1 reaches Library, first visit at its first slot");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 8, "Library's shelf");
    browser.open(Console{"ps2", "PlayStation 2", 2}, 5);
    expect(keyOf(browser.folder()) == "ps2", "the console is open");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 2, "its shelf is its ROMs");
    expect(browser.back() == 5, "back restores Library's focus on the console");
    expect(!browser.folder() && browser.section() == Section::Library &&
               browser.shelf(library(), allReady(), oneAdd(), {}).size() == 8,
           "Library again, not Home");

    browser.open(Console{"ps2", "PlayStation 2", 2}, 5);
    std::vector<Game> gone = library();
    std::erase_if(gone, [](const Game& entry) {
        return entry.sourceId == "ps2";
    });
    expect(browser.shelf(gone, allReady(), oneAdd(), {}).size() == 7 && !browser.folder(),
           "a console whose ROMs are gone returns to Library");

    browser.open(Launcher{Source::Epic, 2, true}, 1);
    expect(browser.inLauncher() && keyOf(browser.folder()) == "launcher:epic",
           "a launcher is open");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 2,
           "its shelf is its store's games");
    expect(browser.back() == 1, "back restores the launcher's slot");

    browser.open(AllGames{4}, 3);
    expect(!browser.inLauncher() && keyOf(browser.folder()) == "all",
           "the combined library is open");
    expect(browser.shelf(library(), allReady(), oneAdd(), {}).size() == 4,
           "its shelf is every title");
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
    const std::optional<opensu::library::Folder> all =
        opensu::library::folderOf(ShelfItem{AllGames{1}});
    expect(all && opensu::library::name(*all) == "All games", "the combined library's name");
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
    [[nodiscard]] std::vector<Game> list() override {
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
    expect(opensu::library::homeShelf(games, {}).size() == 1,
           "Home leaves them to their launchers");
}

int main() try {
    try {
        testHomeShelf();
        testHomeChoices();
        testLibraryShelf();
        testLibraryLaunchers();
        testConsoleShelf();
        testLauncherShelf();
        testRemoteHosts();
        testAllGames();
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

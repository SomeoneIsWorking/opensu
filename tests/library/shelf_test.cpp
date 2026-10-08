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
#include <variant>
#include <vector>

namespace {

using iideck::library::AllGames;
using iideck::library::Availability;
using iideck::library::Catalog;
using iideck::library::Console;
using iideck::library::Game;
using iideck::library::Launcher;
using iideck::library::Section;
using iideck::library::ShelfBrowser;
using iideck::library::ShelfItem;
using iideck::library::Source;
using iideck::library::SourceStatus;

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

std::string keyOf(const std::optional<iideck::library::Folder>& folder) {
    return folder ? iideck::library::key(*folder) : std::string{};
}

std::vector<Source> stores(std::initializer_list<Source> list) {
    return list;
}

void testHomeShelf() {
    const std::vector<ShelfItem> home = iideck::library::homeShelf(library());
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
    expect(iideck::library::homeShelf({}).empty(), "nothing installed is an empty Home");
}

void testLibraryShelf() {
    const std::vector<ShelfItem> shelf = iideck::library::libraryShelf(library(), allReady());
    // Three launchers, All games, two consoles.
    expect(shelf.size() == 6, "launchers, All games and consoles");
    expect(*launcherAt(shelf, 0) == Launcher{Source::Steam, 3, true}, "Steam counts its games");
    expect(*launcherAt(shelf, 1) == Launcher{Source::Epic, 2, true}, "Epic follows Steam");
    expect(*launcherAt(shelf, 2) == Launcher{Source::Gog, 2, true}, "GOG follows Epic");
    expect(std::get_if<AllGames>(&shelf.at(3)) != nullptr &&
               std::get<AllGames>(shelf.at(3)).games == 4,
           "All games counts titles, a title owned in three stores once");
    expect(*consoleAt(shelf, 4) == Console{"gc", "GameCube", 1, {}},
           "GameCube first, in system order");
    expect(*consoleAt(shelf, 5) == Console{"ps2", "PlayStation 2", 2, {}}, "PS2 counts both ROMs");
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
    const std::vector<ShelfItem> shelf = iideck::library::libraryShelf(library(), sources);
    expect(launcherAt(shelf, 0) != nullptr && launcherAt(shelf, 0)->source == Source::Steam &&
               launcherAt(shelf, 1) != nullptr && launcherAt(shelf, 1)->source == Source::Gog,
           "a store that is not there has no tile");
    expect(!launcherAt(shelf, 1)->ready, "a store that needs attention is not ready");
    expect(std::get_if<AllGames>(&shelf.at(2)) != nullptr, "All games follows the stores");

    const std::vector<ShelfItem> bare = iideck::library::libraryShelf({}, {});
    expect(bare.empty(), "nothing at all is an empty Library");

    const std::vector<ShelfItem> romsOnly = iideck::library::libraryShelf(
        {game(Source::Rom, "rom:gc-a", "gc")}, {{Source::Steam, Availability::Absent, {}}});
    expect(romsOnly.size() == 1 && consoleAt(romsOnly, 0) != nullptr,
           "no stores, no launchers and no All games");

    const std::vector<ShelfItem> signedOut = iideck::library::libraryShelf(
        {}, {{Source::Gog, Availability::Attention, "not signed in"}});
    expect(signedOut.size() == 1 && !launcherAt(signedOut, 0)->ready,
           "a signed-out store still has its tile, and no All games without games");
}

void testConsoleShelf() {
    const std::vector<ShelfItem> ps2 = iideck::library::consoleShelf(library(), "ps2");
    expect(ps2.size() == 2 && gameAt(ps2, 0)->id == "rom:ps2-a" &&
               gameAt(ps2, 1)->id == "rom:ps2-b",
           "a console holds its own ROMs in catalog order");
    expect(gameAt(ps2, 0)->ownedIn.empty(), "a ROM shows no stores");
    expect(iideck::library::consoleShelf(library(), "n64").empty(), "no ROMs, empty shelf");
}

void testLauncherShelf() {
    const std::vector<ShelfItem> steam = iideck::library::launcherShelf(library(), Source::Steam);
    expect(steam.size() == 3 && gameAt(steam, 0)->id == "steam:hades" &&
               gameAt(steam, 1)->id == "steam:solo" && gameAt(steam, 2)->id == "steam:celeste",
           "a store holds its whole library, installed or not, in catalog order");
    expect(gameAt(steam, 0)->ownedIn == stores({Source::Gog, Source::Epic}),
           "a game owned elsewhere lists the other stores");
    expect(gameAt(steam, 1)->ownedIn.empty(), "a game owned here alone lists none");
    expect(gameAt(steam, 2)->ownedIn == stores({Source::Epic}) && !gameAt(steam, 2)->installed,
           "the store's own copy is shown even where another store has it installed");

    const std::vector<ShelfItem> gog = iideck::library::launcherShelf(library(), Source::Gog);
    expect(gog.size() == 2 && gameAt(gog, 0)->ownedIn == stores({Source::Steam, Source::Epic}),
           "GOG's Hades lists Steam and Epic");
    expect(iideck::library::launcherShelf({}, Source::Epic).empty(), "no games, empty shelf");
}

void testAllGames() {
    const std::vector<ShelfItem> all = iideck::library::allGamesShelf(library());
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
    expect(browser.shelf(library(), allReady()).size() == 3, "Home's shelf");

    expect(cycleFrom(browser, 1, 2) == 0 && browser.section() == Section::Library,
           "R1 reaches Library, first visit at its first slot");
    expect(browser.shelf(library(), allReady()).size() == 6, "Library's shelf");
    browser.open(Console{"ps2", "PlayStation 2", 2, {}}, 5);
    expect(keyOf(browser.folder()) == "ps2", "the console is open");
    expect(browser.shelf(library(), allReady()).size() == 2, "its shelf is its ROMs");
    expect(browser.back() == 5, "back restores Library's focus on the console");
    expect(!browser.folder() && browser.section() == Section::Library &&
               browser.shelf(library(), allReady()).size() == 6,
           "Library again, not Home");

    browser.open(Console{"ps2", "PlayStation 2", 2, {}}, 5);
    std::vector<Game> gone = library();
    std::erase_if(gone, [](const Game& entry) {
        return entry.sourceId == "ps2";
    });
    expect(browser.shelf(gone, allReady()).size() == 5 && !browser.folder(),
           "a console whose ROMs are gone returns to Library");

    browser.open(Launcher{Source::Epic, 2, true}, 1);
    expect(browser.inLauncher() && keyOf(browser.folder()) == "launcher:epic",
           "a launcher is open");
    expect(browser.shelf(library(), allReady()).size() == 2, "its shelf is its store's games");
    expect(browser.back() == 1, "back restores the launcher's slot");

    browser.open(AllGames{4}, 3);
    expect(!browser.inLauncher() && keyOf(browser.folder()) == "all",
           "the combined library is open");
    expect(browser.shelf(library(), allReady()).size() == 4, "its shelf is every title");
}

void testCycling() {
    ShelfBrowser browser;
    expect(cycleFrom(browser, 1, 2) == 0 && browser.section() == Section::Library,
           "Home to Library");
    expect(cycleFrom(browser, 1, 4) == 2 && browser.section() == Section::Home,
           "Library wraps to Home, which is where it was left");
    expect(cycleFrom(browser, -1, 2) == 4 && browser.section() == Section::Library,
           "L1 from Home wraps to Library, which is where it was left");

    browser.open(Console{"gc", "GameCube", 1, {}}, 4);
    expect(cycleFrom(browser, 1, 0) == 2 && !browser.folder() && browser.section() == Section::Home,
           "leaving Library closes the folder");
    expect(cycleFrom(browser, 1, 2) == 4,
           "Library is remembered at the slot the folder was entered from, not the folder's");
}

void testFolders() {
    expect(!iideck::library::folderOf(ShelfItem{game(Source::Steam, "steam:1")}),
           "a game opens nothing");
    const std::optional<iideck::library::Folder> all =
        iideck::library::folderOf(ShelfItem{AllGames{1}});
    expect(all && iideck::library::name(*all) == "All games", "the combined library's name");
    expect(iideck::library::name(Launcher{Source::Gog, 0, false}) == "GOG",
           "a launcher is its store");
}

class FakeProvider final : public iideck::library::Provider {
  public:
    enum class Mode : std::uint8_t { Lists, Absent, Fails };
    FakeProvider(Source source, Mode mode) : source_{source}, mode_{mode} {
    }
    [[nodiscard]] Source source() const override {
        return source_;
    }
    [[nodiscard]] std::vector<Game> list() override {
        if (mode_ == Mode::Absent) {
            throw iideck::library::SourceAbsent{"not installed"};
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

void testCatalogStatuses() {
    Catalog catalog;
    catalog.add(std::make_unique<FakeProvider>(Source::Steam, FakeProvider::Mode::Lists));
    catalog.add(std::make_unique<FakeProvider>(Source::Epic, FakeProvider::Mode::Fails));
    catalog.add(std::make_unique<FakeProvider>(Source::Gog, FakeProvider::Mode::Absent));
    const iideck::library::CatalogSnapshot snapshot = catalog.refresh();
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

int main() {
    try {
        testHomeShelf();
        testLibraryShelf();
        testLibraryLaunchers();
        testConsoleShelf();
        testLauncherShelf();
        testAllGames();
        testBrowser();
        testCycling();
        testFolders();
        testCatalogStatuses();
        std::printf("shelf: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}

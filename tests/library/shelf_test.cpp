// Shelves: Home's consoles and store games, a console's ROMs, going in and back out; and the
// catalog's per-store status that the launcher badges read.
#include "library/shelf.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

using iideck::library::Availability;
using iideck::library::Catalog;
using iideck::library::Console;
using iideck::library::Game;
using iideck::library::ShelfBrowser;
using iideck::library::ShelfItem;
using iideck::library::Source;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game game(Source source, std::string id, std::string sourceId = {}) {
    Game out;
    out.source = source;
    out.id = std::move(id);
    out.title = out.id;
    out.sourceId = std::move(sourceId);
    out.installed = true;
    return out;
}

std::vector<Game> library() {
    return {game(Source::Steam, "steam:1", "1"),   game(Source::Rom, "rom:ps2-a", "ps2"),
            game(Source::Rom, "rom:gc-a", "gc"),   game(Source::Gog, "gog:2", "2"),
            game(Source::Rom, "rom:ps2-b", "ps2"), game(Source::Steam, "steam:3", "3")};
}

const Console* consoleAt(const std::vector<ShelfItem>& shelf, std::size_t index) {
    return std::get_if<Console>(&shelf.at(index));
}

const Game* gameAt(const std::vector<ShelfItem>& shelf, std::size_t index) {
    return std::get_if<Game>(&shelf.at(index));
}

void testHomeShelf() {
    const std::vector<ShelfItem> home = iideck::library::homeShelf(library());
    expect(home.size() == 5, "two consoles and three store games");
    expect(*consoleAt(home, 0) == Console{"gc", "GameCube", 1, {}},
           "GameCube first, in system order");
    expect(*consoleAt(home, 1) == Console{"ps2", "PlayStation 2", 2, {}}, "PS2 counts both ROMs");
    expect(gameAt(home, 2)->id == "steam:1" && gameAt(home, 3)->id == "gog:2" &&
               gameAt(home, 4)->id == "steam:3",
           "store games follow in catalog order, ROMs left out");
}

void testConsoleShelf() {
    const std::vector<ShelfItem> ps2 = iideck::library::consoleShelf(library(), "ps2");
    expect(ps2.size() == 2 && gameAt(ps2, 0)->id == "rom:ps2-a" &&
               gameAt(ps2, 1)->id == "rom:ps2-b",
           "a console holds its own ROMs in catalog order");
    expect(iideck::library::consoleShelf(library(), "n64").empty(), "no ROMs, empty shelf");
}

void testBrowser() {
    ShelfBrowser browser;
    expect(!browser.back().has_value(), "back on Home goes nowhere");
    browser.open(Console{"ps2", "PlayStation 2", 2, {}}, 1);
    expect(browser.console() && browser.console()->system == "ps2", "the console is open");
    expect(browser.shelf(library()).size() == 2, "its shelf is its ROMs");
    expect(browser.back() == 1, "back restores Home's focus on the console");
    expect(!browser.console() && browser.shelf(library()).size() == 5, "Home again");

    browser.open(Console{"ps2", "PlayStation 2", 2, {}}, 1);
    std::vector<Game> gone = library();
    std::erase_if(gone, [](const Game& entry) {
        return entry.sourceId == "ps2";
    });
    expect(browser.shelf(gone).size() == 4 && !browser.console(),
           "a console whose ROMs are gone returns to Home");
}

class FakeProvider final : public iideck::library::Provider {
  public:
    enum class Mode { Lists, Absent, Fails };
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
    testHomeShelf();
    testConsoleShelf();
    testBrowser();
    testCatalogStatuses();
    std::printf("shelf: all checks passed\n");
    return 0;
}

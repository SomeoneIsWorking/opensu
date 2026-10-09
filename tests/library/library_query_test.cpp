// The library view: text folding, search ranking, each filter, each sort, hidden games and the
// play history that feeds the recent sort.
#include "library/library_query.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include "library/play_history.hpp"
#include "library/text_fold.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu::library;
using opensu::test::expect;

Game game(Source source, std::string id, std::string title, bool installed = true,
          std::string sourceId = {}) {
    Game out;
    out.source = source;
    out.id = std::move(id);
    out.title = std::move(title);
    out.installed = installed;
    out.sourceId = sourceId.empty() ? out.id : std::move(sourceId);
    return out;
}

std::vector<Game> catalog() {
    return {
        game(Source::Steam, "steam:1", "Hades"),
        game(Source::Epic, "epic:celeste", "Celeste", false),
        game(Source::Gog, "gog:tunic", "Tunic", false),
        game(Source::Steam, "steam:2", "Pok\xC3\xA9mon Legends"),
        game(Source::Epic, "epic:ark", "The Ark of Light"),
        game(Source::Rom, "rom:gc:1", "Super Mario Sunshine", true, "gc"),
        game(Source::Rom, "rom:gc:2", "Mario Kart: Double Dash", true, "gc"),
    };
}

std::vector<SourceStatus> sources() {
    return {SourceStatus{Source::Steam, Availability::Ready, {}},
            SourceStatus{Source::Epic, Availability::Ready, {}},
            SourceStatus{Source::Gog, Availability::Ready, {}}};
}

std::vector<std::string> titles(const std::vector<Game>& games) {
    std::vector<std::string> out;
    out.reserve(games.size());
    for (const Game& g : games) {
        out.push_back(g.title);
    }
    return out;
}

std::vector<std::string> gameTitles(const std::vector<ShelfItem>& items) {
    std::vector<std::string> out;
    for (const ShelfItem& item : items) {
        if (const auto* g = std::get_if<Game>(&item)) {
            out.push_back(g->title);
        }
    }
    return out;
}

bool has(const std::vector<std::string>& list, const std::string& title) {
    for (const std::string& entry : list) {
        if (entry == title) {
            return true;
        }
    }
    return false;
}

void foldingIgnoresCaseAccentsAndMarks() {
    expect(foldText("Pok\xC3\xA9mon") == "pokemon", "accents fold");
    expect(foldText("\xC3\x86on  FLUX\t") == "aeon flux", "ligatures, case and white space fold");
    expect(foldText("Zelda\xE2\x84\xA2: Link\xE2\x80\x99s") == "zelda: link's",
           "the trade mark goes and the curly quote reads as an apostrophe");
    expect(foldText("e\xCC\x81") == "e", "a combining mark is dropped");
    expect(foldText("\xE6\x97\xA5\xE6\x9C\xAC") == "\xE6\x97\xA5\xE6\x9C\xAC",
           "other scripts stay");
}

void searchRanksPrefixThenWordThenInside() {
    LibraryView view{catalog(), sources()};
    expect(gameTitles(searchShelf(view, "ark")) == std::vector<std::string>{"The Ark of Light"},
           "a word inside a title is found");
    const auto mario = gameTitles(searchShelf(view, "MARIO"));
    expect(mario.size() == 2 && mario[0] == "Mario Kart: Double Dash",
           "a title that begins with the text outranks one with it as a later word");
    const auto inside = gameTitles(searchShelf(view, "ar"));
    expect(inside.size() == 3 && inside[0] == "The Ark of Light",
           "a word start comes before a match inside a word");
    expect(has(gameTitles(searchShelf(view, "pokemon")), "Pok\xC3\xA9mon Legends"),
           "the plain spelling finds the accented title");
    expect(has(gameTitles(searchShelf(view, "  LEGENDS ")), "Pok\xC3\xA9mon Legends"),
           "case and spaces do not matter");
    expect(searchShelf(view, "").empty(), "no text finds nothing");
    expect(searchShelf(view, "zzzz").empty(), "an unmatched text finds nothing");
}

void searchFindsFoldersBeforeGames() {
    LibraryView view{catalog(), sources()};
    const auto items = searchShelf(view, "steam");
    expect(!items.empty() && !std::holds_alternative<Game>(items.front()),
           "the Steam folder is found by name");
}

void filtersNarrowTheView() {
    const HiddenGames none;
    ViewOptions options;
    options.installedOnly = true;
    const LibraryView installed = applyOptions(catalog(), sources(), options, none);
    expect(installed.games.size() == 5, "uninstalled games drop out");
    expect(!has(titles(installed.games), "Tunic"), "Tunic is not installed");

    options = ViewOptions{};
    options.source = "launcher:epic";
    const LibraryView epic = applyOptions(catalog(), sources(), options, none);
    expect(titles(epic.games).size() == 2, "only Epic's games remain");
    for (const SourceStatus& status : epic.sources) {
        expect((status.source == Source::Epic) == (status.availability != Availability::Absent),
               "the other stores' launchers are left out");
    }

    options.source = "gc";
    const LibraryView gc = applyOptions(catalog(), sources(), options, none);
    expect(gc.games.size() == 2 && gc.games[0].source == Source::Rom, "a system filters its ROMs");

    options = ViewOptions{};
    options.source = "launcher:steam";
    options.installedOnly = true;
    const LibraryView both = applyOptions(catalog(), sources(), options, none);
    expect(both.games.size() == 2, "filters combine");
    expect(narrowing(options) && !narrowing(ViewOptions{}), "narrowing sees a filter");
    ViewOptions sortOnly;
    sortOnly.sort = SortKey::Name;
    expect(!narrowing(sortOnly), "a sort alone does not narrow");
}

void sortsOrderTheView() {
    const HiddenGames none;
    ViewOptions options;
    options.sort = SortKey::Name;
    const auto byName = titles(applyOptions(catalog(), sources(), options, none).games);
    expect(byName[0] == "Celeste", "name order starts alphabetically");
    expect(byName.back() == "Tunic", "and ends with Tunic");
    expect(byName[3] == "Pok\xC3\xA9mon Legends", "an accented title sorts as its plain spelling");

    options.sort = SortKey::Store;
    const LibraryView byStore = applyOptions(catalog(), sources(), options, none);
    expect(byStore.games.front().source == Source::Steam, "Steam first");
    expect(byStore.games.back().source == Source::Rom, "ROMs last");

    std::vector<Game> played = catalog();
    played[4].lastPlayed = std::chrono::system_clock::now();
    options.sort = SortKey::Recent;
    expect(applyOptions(played, sources(), options, none).games.front().title == "The Ark of Light",
           "recently played comes first");
    for (const SortKey sort : allSortKeys) {
        expect(sortKeyOf(key(sort)) == sort, "a sort round-trips through its key");
    }
    expect(!sortKeyOf("bogus"), "an unknown key is nothing");
}

void hiddenGamesLeaveEverywhereButTheHiddenFilter() {
    HiddenGames hidden;
    const std::vector<Game> games = catalog();
    hidden.set(games[0], true);
    hidden.set(games[5], true);
    expect(hidden.contains(games[0]) && hidden.contains(games[5]), "hidden games are known");
    Game epicCopy = game(Source::Epic, "epic:hades", "HADES");
    expect(hidden.contains(epicCopy), "a store title is hidden in every store");
    expect(!hidden.contains(games[6]), "another ROM is not hidden");

    ViewOptions options;
    const LibraryView shown = applyOptions(games, sources(), options, hidden);
    expect(!has(titles(shown.games), "Hades") && !has(titles(shown.games), "Super Mario Sunshine"),
           "hidden games leave the view");
    expect(gameTitles(searchShelf(shown, "hades")).empty(), "and search");
    expect(shown.games.size() == 5, "the rest stay");

    options.hiddenOnly = true;
    const LibraryView only = applyOptions(games, sources(), options, hidden);
    expect(only.games.size() == 2, "the hidden filter shows only the hidden games");
    expect(!gameTitles(searchShelf(only, "hades")).empty(), "and search finds them there");

    hidden.set(games[0], false);
    expect(!hidden.contains(games[0]) && hidden.keys().size() == 1, "unhide removes the key");
    hidden.set(games[0], false);
    expect(hidden.keys().size() == 1, "unhiding twice changes nothing");
    expect(HiddenGames{{"id:b", "id:a", "id:a"}}.keys() ==
               std::vector<std::string>({"id:a", "id:b"}),
           "keys are sorted and unique");
}

void sourceChoicesListWhatIsPresent() {
    const auto choices = sourceChoices(catalog(), sources());
    expect(choices.front().key.empty() && choices.front().label == "All sources", "all first");
    bool epic = false;
    bool gc = false;
    for (const SourceChoice& choice : choices) {
        epic = epic || choice.key == "launcher:epic";
        gc = gc || choice.key == "gc";
    }
    expect(epic && gc, "a store and a system with ROMs are choices");
}

void resultsDescribeTheirOwner() {
    Game owned = game(Source::Steam, "steam:9", "Hades");
    owned.ownedIn = {Source::Steam, Source::Gog};
    expect(describeResult(owned).find("Steam") == 0 &&
               describeResult(owned).find("GOG") != std::string::npos,
           "a game lists its stores");
    expect(describeResult(game(Source::Rom, "r", "x", true, "gc")) != "gc",
           "a ROM names its system");
    expect(describeResult(Launcher{Source::Epic, 0, true, false}) == "Store",
           "a launcher is a store");
}

void visibleShelfSearchesOrBrowses() {
    ShelfBrowser browser;
    ViewOptions options;
    const HiddenGames none;
    const auto browse = visibleShelf(browser, catalog(), sources(), options, none);
    options.search = "tunic";
    const auto search = visibleShelf(browser, catalog(), sources(), options, none);
    expect(gameTitles(search) == std::vector<std::string>{"Tunic"}, "a search lists its results");
    expect(browse.size() != search.size(), "and the browser's shelf otherwise");
}

void playHistoryKeepsTheLaterTime() {
    using namespace std::chrono;
    PlayHistory history;
    const auto now = system_clock::now();
    history.record("epic:celeste", now);
    history.record("steam:1", now - hours{100});
    std::vector<Game> games = catalog();
    games[0].lastPlayed = now - hours{1};
    games[1].installed = true;
    history.apply(games);
    expect(
        duration_cast<seconds>(opensu::test::need(games[1].lastPlayed, "a time") - now).count() ==
            0,
        "a game with no time takes the recorded one");
    expect(opensu::test::need(games[0].lastPlayed, "a time") > now - hours{2},
           "a later store time is kept");
    expect(history.entries().size() == 2, "entries are kept by id");
    expect(PlayHistory{history.entries()} == history, "entries round-trip");
}

} // namespace

int main() {
    foldingIgnoresCaseAccentsAndMarks();
    searchRanksPrefixThenWordThenInside();
    searchFindsFoldersBeforeGames();
    filtersNarrowTheView();
    sortsOrderTheView();
    hiddenGamesLeaveEverywhereButTheHiddenFilter();
    sourceChoicesListWhatIsPresent();
    resultsDescribeTheirOwner();
    visibleShelfSearchesOrBrowses();
    playHistoryKeepsTheLaterTime();
    std::printf("library_query: all checks passed\n");
    return 0;
}

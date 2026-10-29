// Play time reported by emulators, merged into the library; and an emulator's game list matched
// to openSU's games by file, else by a title only one game has.
#include <cstdio>
#include <string>
#include <vector>

#include "library/emulator_game_list.hpp"
#include "library/play_time.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::library;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Game game(const std::string& id, Source source, const std::string& title, const char* file = "") {
    Game made;
    made.id = id;
    made.source = source;
    made.title = title;
    made.file = file;
    return made;
}

void sourcesSumPerGame() {
    std::vector<Game> games{game("rom:a", Source::Rom, "A"), game("rom:b", Source::Rom, "B"),
                            game("steam:1", Source::Steam, "C")};
    games[2].playtimeMinutes = 600;
    PlayTimeReports reports;
    reports.set("eden", {{"rom:a", 3600}, {"steam:1", 60}});
    reports.set("ryujinx", {{"rom:a", 1830}});
    reports.apply(games);
    expect(games[0].playtimeMinutes == 90, "two emulators' seconds add up, then whole minutes");
    expect(games[1].playtimeMinutes == 0, "a game no source reports has none");
    expect(games[2].playtimeMinutes == 600, "a store game keeps the store's value");
    reports.set("eden", {{"rom:a", 3600}});
    reports.apply(games);
    expect(games[0].playtimeMinutes == 90, "a source's new answer replaces its old one");
    reports.erase("ryujinx");
    reports.apply(games);
    expect(games[0].playtimeMinutes == 60, "a source turned off no longer counts");
}

void gameListMatchesByFileThenTitle() {
    const std::vector<Game> games{
        game("rom:1", Source::Rom, "Metroid Dread", "/roms/switch/x/../Dread.nsp"),
        game("rom:2", Source::Rom, "Mario Kart 8 Deluxe", "/roms/switch/mk8.nsp"),
        game("rom:3", Source::Rom, "Tetris", "/roms/switch/tetris-a.nsp"),
        game("rom:4", Source::Rom, "TETRIS", "/roms/switch/tetris-b.nsp"),
        game("rom:5", Source::Rom, "Unlisted", "/roms/switch/u.nsp")};
    const std::vector<EmulatorGameEntry> entries{
        {"0100A", "Something Else", "/roms/switch/Dread.nsp", {}},
        {"0100B", "Mario Kart 8 Deluxe", "", {}},
        {"0100C", "Tetris", "", {}},
        {"0100D", "", "", {{"versions", "Update"}}}};
    const auto matched = matchGameList(games, entries);
    expect(matched.at("rom:1") == 0, "the same file matches whatever the titles say");
    expect(matched.at("rom:2") == 1, "a title one game and one entry share matches");
    expect(!matched.contains("rom:3") && !matched.contains("rom:4"),
           "a title two games share matches neither");
    expect(!matched.contains("rom:5") && matched.size() == 2, "nothing else matches");
}

} // namespace

int main() try {
    sourcesSumPerGame();
    gameListMatchesByFileThenTitle();
    std::printf("play_time: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

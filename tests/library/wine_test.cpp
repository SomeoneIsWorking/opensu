// The Wine runner and prefix of Windows GOG and Epic launches, against a fake PATH and fake Steam
// folders under the test scratch: what is found, the shared and own prefixes, the arguments added,
// and the refusals.
#include "library/wine.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::Game;
using opensu::library::LaunchSpec;
using opensu::library::Source;
namespace wine = opensu::library::wine;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path binary(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream{path} << "#!/bin/sh\nexit 0\n";
    fs::permissions(path, fs::perms::owner_all);
    return path;
}

Game windowsGame(Source source, const std::string& id) {
    Game game;
    game.source = source;
    game.sourceId = id;
    game.id = std::string{source == Source::Gog ? "gog:" : "epic:"} + id;
    game.title = "Game " + id;
    game.installed = true;
    game.windowsInstall = true;
    game.launch = LaunchSpec{.program = source == Source::Gog ? "gogdl" : "legendary",
                             .args = {"launch", id}};
    return game;
}

void runnersAreFound(const fs::path& dir) {
    const fs::path bin = dir / "bin";
    const fs::path system = binary(bin / "wine");
    const fs::path root = dir / "steam";
    const fs::path ge = binary(root / "compatibilitytools.d/GE-Proton9-20/files/bin/wine");
    binary(root / "compatibilitytools.d/Broken/proton");
    const fs::path library = dir / "library";
    const fs::path old = binary(library / "steamapps/common/Proton 7.0/dist/bin/wine");
    const fs::path current = binary(library / "steamapps/common/Proton 9.0 (Beta)/files/bin/wine");
    binary(library / "steamapps/common/Other Game/files/bin/wine");

    const std::vector<wine::Runner> found = wine::discover(
        wine::Search{.executablePath = {bin}, .steamRoots = {root}, .steamLibraries = {library}});
    expect(found == std::vector<wine::Runner>{{"System Wine", system},
                                              {"GE-Proton9-20", ge},
                                              {"Proton 7.0", old},
                                              {"Proton 9.0 (Beta)", current}},
           "system Wine, then compatibility tools, then Steam's Proton builds, each by name");
    expect(wine::discover(wine::Search{}).empty(), "nothing searched finds nothing");
    expect(wine::discover(wine::Search{.executablePath = {},
                                       .steamRoots = {root, root},
                                       .steamLibraries = {library, library}})
                   .size() == 3,
           "a folder searched twice lists each runner once");
}

void prefixesAreSharedUnlessOwn(const fs::path& dir) {
    const fs::path data = dir / "data";
    wine::OwnPrefixes own;
    const Game gog = windowsGame(Source::Gog, "1207658930");
    const Game epic = windowsGame(Source::Epic, "Sugar/../Free");
    expect(wine::prefixFor(data, gog, own) == data / "wine-prefix", "by default one shared prefix");
    own.set(gog.id, true);
    expect(wine::prefixFor(data, gog, own) == data / "wine-prefixes/gog-1207658930",
           "a game set to its own gets <store>-<id>");
    expect(wine::prefixFor(data, epic, own) == data / "wine-prefix", "others still share");
    expect(wine::ownPrefix(data, epic) == data / "wine-prefixes/epic-Sugar_.._Free",
           "an id cannot leave the prefixes folder");
    own.set(gog.id, false);
    expect(!own.contains(gog.id), "set back to shared");
}

void launchesGetTheRunnerAndPrefix(const fs::path& dir) {
    const fs::path data = dir / "data";
    const fs::path runner = binary(dir / "runners/wine");
    wine::Choice choice{.runner = {},
                        .found = {{"System Wine", runner}},
                        .dataDir = data,
                        .own = wine::OwnPrefixes{{"epic:Fortnite"}}};

    Game gog = windowsGame(Source::Gog, "2");
    expect(wine::prepare(gog, choice).empty(), "a Windows GOG install is prepared");
    expect(gog.launch.args == std::vector<std::string>{"launch", "2", "--wine", runner.string(),
                                                       "--wine-prefix",
                                                       (data / "wine-prefix").string()},
           "gogdl gets the first runner found and the shared prefix");
    expect(fs::is_directory(data / "wine-prefix"), "the shared prefix folder is made");

    Game epic = windowsGame(Source::Epic, "Fortnite");
    expect(wine::prepare(epic, choice).empty(), "a Windows Epic install is prepared");
    expect(epic.launch.args ==
               std::vector<std::string>{"launch", "Fortnite", "--wine", runner.string(),
                                        "--wine-prefix",
                                        (data / "wine-prefixes/epic-Fortnite").string()},
           "legendary gets --wine and the game's own prefix");

    Game linux = windowsGame(Source::Gog, "3");
    linux.windowsInstall = false;
    Game steam = windowsGame(Source::Steam, "10");
    steam.launch = LaunchSpec{.program = "steam", .args = {"-applaunch", "10"}};
    expect(wine::prepare(linux, choice).empty() &&
               linux.launch.args == std::vector<std::string>{"launch", "3"},
           "a Linux install is left as it is");
    expect(wine::prepare(steam, choice).empty() &&
               steam.launch.args == std::vector<std::string>{"-applaunch", "10"},
           "a Steam game is left to Steam");

    const fs::path picked = binary(dir / "runners/proton-wine");
    choice.runner = picked;
    Game chosen = windowsGame(Source::Gog, "4");
    expect(wine::prepare(chosen, choice).empty() && chosen.launch.args[3] == picked.string(),
           "the player's runner wins over the first found");

    choice.runner = dir / "runners/gone";
    Game gone = windowsGame(Source::Gog, "5");
    expect(wine::prepare(gone, choice) == "The Wine runner " + (dir / "runners/gone").string() +
                                              " is gone; pick another in Settings" &&
               gone.launch.args.size() == 2,
           "a missing runner refuses the launch and leaves it untouched");

    choice.runner.clear();
    choice.found.clear();
    Game none = windowsGame(Source::Epic, "Owned");
    expect(wine::prepare(none, choice) ==
               "No Wine found to run Game Owned; install Wine or a Proton build",
           "no runner at all refuses the launch");
}

} // namespace

int main() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "wine"};
    runnersAreFound(scratch.path() / "found");
    prefixesAreSharedUnlessOwn(scratch.path() / "prefixes");
    launchesGetTheRunnerAndPrefix(scratch.path() / "launch");
    std::printf("wine: all checks passed\n");
    return 0;
}

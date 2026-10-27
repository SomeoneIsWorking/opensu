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
    const fs::path lutris = dir / "lutris";
    const fs::path lutrisWine = binary(lutris / "runners/wine/wine-ge-8-26-x86_64/bin/wine");

    const std::vector<wine::Runner> found = wine::discover(wine::Search{.executablePath = {bin},
                                                                        .steamRoots = {root},
                                                                        .steamLibraries = {library},
                                                                        .lutris = lutris});
    const auto steam = [](const char* name, const fs::path& wine) {
        // No `proton` script in these fakes: each is taken as its plain Wine.
        return wine::Runner{
            .name = name, .wine = wine, .proton = {}, .origin = wine::Origin::Steam};
    };
    expect(found == std::vector<wine::Runner>{steam("GE-Proton9-20", ge),
                                              steam("Proton 9.0 (Beta)", current),
                                              steam("Proton 7.0", old),
                                              {.name = "wine-ge-8-26-x86_64",
                                               .wine = lutrisWine,
                                               .origin = wine::Origin::Lutris},
                                              {.name = "System Wine", .wine = system}},
           "Steam's Proton builds newest first, Lutris's Wine builds, then system Wine");
    expect(wine::discover(wine::Search{}).empty(), "nothing searched finds nothing");
    expect(wine::discover(wine::Search{.executablePath = {},
                                       .steamRoots = {root, root},
                                       .steamLibraries = {library, library}})
                   .size() == 3,
           "a folder searched twice lists each runner once");
}

void automaticTakesTheFirstThatRuns(const fs::path& dir) {
    const wine::Runner ours{.name = "GE-Proton11-11",
                            .wine = dir / "ours/files/bin/wine",
                            .proton = dir / "ours",
                            .origin = wine::Origin::OpenSu};
    const wine::Runner steam{.name = "Proton 9.0",
                             .wine = dir / "steam/files/bin/wine",
                             .proton = dir / "steam",
                             .origin = wine::Origin::Steam};
    const wine::Runner lutris{
        .name = "wine-ge-8-26", .wine = dir / "lutris/bin/wine", .origin = wine::Origin::Lutris};
    const wine::Runner system{.name = "System Wine", .wine = dir / "bin/wine"};
    wine::Choice choice{.runner = {},
                        .pins = {},
                        .found = {ours, steam, lutris, system},
                        .lutris = lutris,
                        .dataDir = dir,
                        .own = {},
                        .umuRun = dir / "umu-run"};
    const auto reason = [&choice]() {
        return wine::resolve(choice, "gog:1")->reason;
    };
    expect(reason() == wine::Reason::OpenSuGeProton, "openSU's GE-Proton first");
    choice.found = {steam, lutris, system};
    expect(reason() == wine::Reason::SteamProton && wine::runnerOf(choice, "gog:1") == steam,
           "no GE-Proton: Steam's Proton");
    choice.found = {lutris, system};
    expect(reason() == wine::Reason::Lutris && wine::runnerOf(choice, "gog:1") == lutris,
           "neither: what Lutris uses");
    choice.lutris.reset();
    expect(reason() == wine::Reason::SystemWine, "none of those: system Wine");
    choice.found = {ours, steam, system};
    choice.umuRun.clear();
    expect(reason() == wine::Reason::SystemWine, "a Proton build needs a umu-run");
    choice.pins = {{"gog:1", steam.wine}};
    expect(reason() == wine::Reason::Pinned && wine::runnerOf(choice, "gog:1") == steam,
           "a pin wins over Automatic");
    choice.found.clear();
    expect(!wine::resolve(choice, "gog:2"), "nothing found: nothing");
}

void lutrisDefaultIsRead(const fs::path& dir) {
    const fs::path data = dir / "lutris";
    const wine::Runner ge{.name = "GE-Proton11-5",
                          .wine = dir / "Steam/compatibilitytools.d/GE-Proton11-5/files/bin/wine",
                          .proton = dir / "Steam/compatibilitytools.d/GE-Proton11-5",
                          .origin = wine::Origin::Steam};
    const wine::Runner valve{.name = "Proton 10.0",
                             .wine = dir / "common/Proton 10.0/files/bin/wine",
                             .proton = dir / "common/Proton 10.0",
                             .origin = wine::Origin::Steam};
    const wine::Runner system{.name = "System Wine", .wine = dir / "bin/wine"};
    fs::create_directories(data / "runners");
    std::ofstream{data / "runners/wine.yml"} << "{}\n";
    expect(wine::lutrisDefault(data, {valve, ge, system}) == ge,
           "no version set: Lutris's latest GE-Proton, the one in compatibilitytools.d");
    expect(!wine::lutrisDefault(data, {valve, system}),
           "no GE-Proton on disk: nothing, as Lutris would have umu download one");
    std::ofstream{data / "runners/wine.yml"} << "wine:\n  version: system\nsystem:\n  x: 1\n";
    expect(wine::lutrisDefault(data, {valve, ge, system}) == system, "\"system\" is system Wine");
    std::ofstream{data / "runners/wine.yml"} << "wine:\n  version: 'Proton 10.0'\n";
    expect(wine::lutrisDefault(data, {valve, ge, system}) == valve, "a version by name");
    expect(!wine::lutrisDefault({}, {valve, ge, system}), "no Lutris folder: nothing");
}

void prefixesAreReadiedForTheRunner(const fs::path& dir) {
    const wine::Runner proton{.name = "GE-Proton11-11",
                              .wine = dir / "ge/files/bin/wine",
                              .proton = dir / "ge",
                              .origin = wine::Origin::OpenSu};
    const wine::Runner plain{.name = "System Wine", .wine = dir / "bin/wine"};
    const fs::path prefix = dir / "wine-prefix";
    expect(wine::readyPrefix(prefix, proton).prefix == prefix && fs::is_directory(prefix) &&
               !fs::exists(dir / "wine-prefix.system-wine-saves"),
           "a new prefix is made, with nothing to keep");
    fs::create_directories(prefix / "drive_c/users/me");
    std::ofstream{prefix / "drive_c/users/me/save.dat"} << "1";
    std::ofstream{prefix / "system.reg"} << "WINE REGISTRY Version 2\n";
    expect(wine::readyPrefix(prefix, plain).prefix == prefix &&
               !fs::exists(dir / "wine-prefix.plain-wine"),
           "plain Wine runs in a prefix plain Wine made");
    const wine::ReadyPrefix toProton = wine::readyPrefix(prefix, proton);
    expect(toProton.refusal.empty() && toProton.prefix == prefix &&
               fs::exists(dir / "wine-prefix.system-wine-saves/drive_c/users/me/save.dat") &&
               fs::exists(dir / "wine-prefix.system-wine-saves/system.reg"),
           "Proton runs in it too, its saves copied first");
    fs::create_directory_symlink(".", prefix / "pfx");
    fs::create_directory_symlink("me", prefix / "drive_c/users/steamuser");
    const wine::ReadyPrefix toWine = wine::readyPrefix(prefix, plain);
    expect(toWine.refusal.empty() && toWine.prefix == dir / "wine-prefix.plain-wine" &&
               fs::exists(dir / "wine-prefix.plain-wine/drive_c/users/me/save.dat") &&
               fs::is_symlink(dir / "wine-prefix.plain-wine/drive_c/users/steamuser") &&
               !fs::exists(dir / "wine-prefix.plain-wine/system.reg"),
           "plain Wine never runs in a Proton prefix: it gets its own, starting from its saves");
    std::ofstream{prefix / "drive_c/users/me/save.dat"} << "2";
    static_cast<void>(wine::readyPrefix(prefix, plain));
    std::ifstream kept{dir / "wine-prefix.plain-wine/drive_c/users/me/save.dat"};
    std::string line;
    std::getline(kept, line);
    expect(line == "1", "and copies them only the first time");
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
    wine::Choice choice{.runner = runner,
                        .found = {{"System Wine", runner}},
                        .dataDir = data,
                        .own = wine::OwnPrefixes{{"epic:Fortnite"}}};

    Game gog = windowsGame(Source::Gog, "2");
    expect(wine::prepare(gog, choice).refusal.empty(), "a Windows GOG install is prepared");
    expect(gog.launch.args == std::vector<std::string>{"launch", "2", "--wine", runner.string(),
                                                       "--wine-prefix",
                                                       (data / "wine-prefix").string()},
           "gogdl gets the picked plain Wine and the shared prefix");
    expect(fs::is_directory(data / "wine-prefix"), "the shared prefix folder is made");

    Game epic = windowsGame(Source::Epic, "Fortnite");
    expect(wine::prepare(epic, choice).refusal.empty(), "a Windows Epic install is prepared");
    expect(epic.launch.args ==
               std::vector<std::string>{"launch", "Fortnite", "--wine", runner.string(),
                                        "--wine-prefix",
                                        (data / "wine-prefixes/epic-Fortnite").string()},
           "legendary gets --wine and the game's own prefix");

    Game linux = windowsGame(Source::Gog, "3");
    linux.windowsInstall = false;
    Game steam = windowsGame(Source::Steam, "10");
    steam.launch = LaunchSpec{.program = "steam", .args = {"-applaunch", "10"}};
    expect(wine::prepare(linux, choice).refusal.empty() &&
               linux.launch.args == std::vector<std::string>{"launch", "3"},
           "a Linux install is left as it is");
    expect(wine::prepare(steam, choice).refusal.empty() &&
               steam.launch.args == std::vector<std::string>{"-applaunch", "10"},
           "a Steam game is left to Steam");

    const fs::path picked = binary(dir / "runners/proton-wine");
    choice.runner = picked;
    Game chosen = windowsGame(Source::Gog, "4");
    expect(wine::prepare(chosen, choice).refusal.empty() &&
               chosen.launch.args[3] == picked.string(),
           "the player's runner wins over the first found");

    choice.runner = dir / "runners/gone";
    Game gone = windowsGame(Source::Gog, "5");
    expect(wine::prepare(gone, choice).refusal ==
                   "The runner picked for Game 5 is no longer installed; pick another in "
                   "Settings › Wine" &&
               gone.launch.args.size() == 2,
           "a missing runner refuses the launch and leaves it untouched");

    choice.runner.clear();
    choice.found.clear();
    Game none = windowsGame(Source::Epic, "Owned");
    expect(wine::prepare(none, choice).refusal ==
               "Game Owned cannot start: no Wine or Proton is installed",
           "nothing Automatic can run refuses the launch");
}

} // namespace

void namesReadAsWords() {
    expect(wine::displayName("GE-Proton11-5-x86_64") == "GE-Proton 11-5", "CPU suffix dropped");
    expect(wine::displayName("GE-Proton11-11") == "GE-Proton 11-11", "a space before the version");
    expect(wine::displayName("Proton 9.0 (Beta)") == "Proton 9.0 Beta", "no parentheses");
    expect(wine::displayName("wine-ge-8-26-x86_64") == "wine-ge 8-26", "a Lutris build");
    expect(wine::displayName("System Wine") == "System Wine" &&
               wine::displayName("Proton Experimental") == "Proton Experimental",
           "names without a version stay");
}

int main() {
    namesReadAsWords();
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "wine"};
    runnersAreFound(scratch.path() / "found");
    automaticTakesTheFirstThatRuns(scratch.path() / "automatic");
    lutrisDefaultIsRead(scratch.path() / "lutris");
    prefixesAreReadiedForTheRunner(scratch.path() / "saves");
    prefixesAreSharedUnlessOwn(scratch.path() / "prefixes");
    launchesGetTheRunnerAndPrefix(scratch.path() / "launch");
    std::printf("wine: all checks passed\n");
    return 0;
}

// The player's own games: the records kept in openSU's data dir (ids never given twice), what a
// file is, the launch each kind gets (an added .exe through `wine::prepare` in its
// own prefix), and the imports from Steam's shortcuts.vdf and Lutris's pga.db, both read only.
#include "library/own_games.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "library/lutris_games.hpp"
#include "library/steam_shortcuts.hpp"
#include "library/wine.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "vdf/binary.hpp"

namespace {

namespace fs = std::filesystem;
namespace own = opensu::library::own;
namespace wine = opensu::library::wine;
using opensu::library::Game;
using opensu::library::Source;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path file(const fs::path& path, const std::string& text, bool executable) {
    fs::create_directories(path.parent_path());
    std::ofstream{path} << text;
    fs::permissions(path, executable ? fs::perms::owner_all
                                     : fs::perms::owner_read | fs::perms::owner_write);
    return path;
}

void recordsKeepEntries(const fs::path& dir) {
    const own::Records records{own::recordsFile(dir)};
    expect(records.all().empty(), "no file is no games");
    const std::string first = records.add({.id = {}, .title = "A", .file = dir / "a"});
    const std::string second = records.add({.id = {}, .title = "B", .file = dir / "b"});
    expect(first == "1" && second == "2", "ids count up");
    expect(records.rename("1", "Alpha") && records.setFile("2", dir / "c"), "edits find entries");
    expect(!records.rename("9", "x") && !records.remove("9"), "an unknown id changes nothing");
    expect(records.remove("2"), "an entry is removed");
    const std::string third = records.add({.id = {}, .title = "C", .file = dir / "d"});
    expect(third == "3", "a removed game's id is never given again");
    records.add({.id = {}, .title = "D", .file = dir / "e", .steamAppId = "1245620"});
    const std::vector<own::Entry> all = records.all();
    expect(all.size() == 3 && all[0].title == "Alpha" && all[1].id == "3" &&
               all[2].steamAppId == "1245620" && own::gameOf(all[2]).artworkKey == "1245620",
           "the file reads back, the Steam app id as the game's artwork key");

    std::ofstream{own::recordsFile(dir)} << "[1, 2]";
    bool threw = false;
    try {
        static_cast<void>(records.all());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw, "a file that is not a list of games is an error, not an empty library");
}

void filesAreKnownAndTitled(const fs::path& dir) {
    const fs::path program = file(dir / "Celeste/Celeste", "x", true);
    const fs::path data = file(dir / "Celeste/data.bin", "x", false);
    const fs::path script = file(dir / "Some Game/start.sh", "#!/bin/bash\n", false);
    const fs::path appimage = file(dir / "Tool.AppImage", "x", false);
    const fs::path exe = file(dir / "Win/Hollow.EXE", "MZ", false);
    expect(own::kindOf(program) == own::Kind::Native && own::kindOf(script) == own::Kind::Script &&
               own::kindOf(appimage) == own::Kind::AppImage &&
               own::kindOf(exe) == own::Kind::Windows,
           "each kind by extension and execute bit");
    expect(!own::kindOf(data) && !own::kindOf(dir), "a data file and a folder are not games");
    expect(own::refusal(program).empty() && !own::refusal(data).empty() &&
               !own::refusal(dir / "missing").empty(),
           "only a game file is accepted");
}

void launchesFollowTheKind(const fs::path& dir) {
    const fs::path program = file(dir / "Native/game.x86_64", "x", true);
    Game native = own::gameOf({.id = "1", .title = "Native", .file = program});
    expect(native.id == "own:1" && native.source == Source::Own && native.installed,
           "an entry is an installed game of its own source");
    expect(native.launch.program == program.string() && native.launch.args.empty() &&
               native.launch.workingDirectory == program.parent_path(),
           "a program starts itself in its folder");

    const fs::path script = file(dir / "Script/run.sh", "#!/usr/bin/env bash\necho\n", false);
    const Game scripted = own::gameOf({.id = "2", .title = "S", .file = script});
    expect(scripted.launch.program == "/usr/bin/env" &&
               scripted.launch.args == std::vector<std::string>{"bash", script.string()},
           "a script that is not executable runs through its #! interpreter");

    const fs::path appimage = file(dir / "App.AppImage", "x", false);
    const Game blocked = own::gameOf({.id = "3", .title = "App", .file = appimage});
    expect(blocked.launch.empty() && !blocked.unavailable.empty(),
           "an AppImage that is not executable says so instead of starting");

    const Game gone = own::gameOf({.id = "4", .title = "Gone", .file = dir / "gone"});
    expect(gone.installed && gone.launch.empty() &&
               gone.unavailable.find("Change file") != std::string::npos,
           "a file that is gone points at Change file");

    const Game lutris = own::gameOf(
        {.id = "5", .title = "L", .file = {}, .origin = own::Origin::Lutris, .originId = "7"});
    expect(lutris.launch.program == "lutris" &&
               lutris.launch.args == std::vector<std::string>{"lutris:rungameid/7"},
           "a Lutris game runs through Lutris");
}

void windowsRunsInItsOwnPrefix(const fs::path& dir) {
    const fs::path exe = file(dir / "Win/Game.exe", "MZ", false);
    const fs::path runner = file(dir / "runners/wine", "#!/bin/sh\n", true);
    const fs::path data = dir / "data";
    Game game = own::gameOf({.id = "6", .title = "Win", .file = exe});
    expect(game.windowsInstall, "an .exe runs under Wine");
    const wine::Choice plain{.found = {{"System Wine", runner}}, .dataDir = data};
    const wine::Prepared prepared = wine::prepare(game, plain);
    const fs::path prefix = data / "wine-prefixes/own-6";
    expect(prepared.refusal.empty() && game.launch.program == runner.string() &&
               game.launch.args == std::vector<std::string>{exe.string()} &&
               game.launch.workingDirectory == exe.parent_path(),
           "plain Wine starts the .exe itself in its folder");
    expect(prepared.environment == std::vector<std::string>{"WINEPREFIX=" + prefix.string()} &&
               fs::is_directory(prefix),
           "in the game's own prefix, never the shared one");

    const fs::path build = dir / "GE-Proton11-5";
    const fs::path protonWine = file(build / "files/bin/wine", "#!/bin/sh\n", true);
    file(build / "proton", "#!/bin/sh\n", true);
    const fs::path umu = file(dir / "umu-run", "#!/bin/sh\n", true);
    Game proton = own::gameOf({.id = "7", .title = "Win", .file = exe});
    const wine::Choice viaUmu{.found = {{.name = "GE-Proton11-5",
                                         .wine = protonWine,
                                         .proton = build,
                                         .origin = wine::Origin::OpenSu}},
                              .dataDir = data,
                              .umuRun = umu};
    const wine::Prepared umuRun = wine::prepare(proton, viaUmu);
    expect(umuRun.refusal.empty() && proton.launch.program == umu.string() &&
               proton.launch.args == std::vector<std::string>{exe.string()},
           "a Proton build starts the .exe through umu-run");
    expect(std::ranges::find(umuRun.environment, "PROTONPATH=" + build.string()) !=
                   umuRun.environment.end() &&
               std::ranges::find(umuRun.environment,
                                 "WINEPREFIX=" + (data / "wine-prefixes/own-7").string()) !=
                   umuRun.environment.end(),
           "with umu's environment and the game's own prefix");
}

/// One shortcut as Steam's binary KeyValues writes it.
std::string shortcut(const std::string& index, std::int32_t appId, const std::string& name,
                     const std::string& exe, const std::string& start, const std::string& options) {
    const auto text = [](const std::string& key, const std::string& value) {
        return std::string{"\x01"} + key + '\0' + value + '\0';
    };
    std::string out = std::string{"\x00", 1} + index + '\0';
    out += std::string{"\x02"} + "appid" + '\0';
    for (int i = 0; i < 4; ++i) {
        out += static_cast<char>((static_cast<std::uint32_t>(appId) >> (8 * i)) & 0xFFU);
    }
    out += text("AppName", name) + text("Exe", exe) + text("StartDir", start) +
           text("LaunchOptions", options);
    out += std::string{"\x00", 1} + "tags" + '\0' + "\x08";
    return out + "\x08";
}

void steamShortcutsAreRead(const fs::path& dir) {
    const fs::path root = dir / "Steam";
    const std::string bytes =
        std::string{"\x00", 1} + "shortcuts" + '\0' +
        shortcut("0", -1395302980, "Eden", "\"/apps/eden.appimage\"", "\"/apps/\"", "") +
        shortcut("1", 12, "RE4", "\"/x/re4.sh\"", "\"/x/\"",
                 "PROTON_LOG=1 %command% -windowed \"two words\"") +
        "\x08\x08";
    fs::create_directories(root / "userdata/1/config");
    std::ofstream{root / "userdata/1/config/shortcuts.vdf", std::ios::binary} << bytes;
    const std::optional<opensu::vdf::Node> parsed = opensu::vdf::parseBinary(bytes);
    expect(parsed && parsed->str({"shortcuts", "1", "AppName"}) == "RE4", "binary KeyValues parse");
    expect(!opensu::vdf::parseBinary(std::string{"\x00", 1} + "open"),
           "a cut-off file does not parse");

    const std::vector<own::Entry> found = own::steamShortcuts({root, root});
    expect(found.size() == 2, "each shortcut once, however many roots name it");
    expect(found[0].title == "Eden" && found[0].file == "/apps/eden.appimage" &&
               found[0].workingDirectory == "/apps/" && found[0].originId == "2899664316" &&
               found[0].origin == own::Origin::SteamShortcut,
           "quotes dropped and the app id unsigned");
    expect(found[1].args == std::vector<std::string>{"-windowed", "two words"},
           "launch options after %command%, quoted words kept whole");
}

void lutrisGamesAreRead(const fs::path& dir) {
    expect(own::lutrisGames(dir / "none").empty(), "no Lutris is no games");
    fs::create_directories(dir / "lutris");
    const fs::path db = dir / "lutris/pga.db";
    sqlite3* database = nullptr;
    expect(sqlite3_open(db.string().c_str(), &database) == SQLITE_OK, "a fake pga.db is made");
    expect(sqlite3_exec(database,
                        "CREATE TABLE games (id INTEGER PRIMARY KEY, name TEXT, installed INTEGER);"
                        "INSERT INTO games VALUES (2, 'Lies of P', 1), (3, 'Gone', 0),"
                        "(9, 'Elden Ring', 1);",
                        nullptr, nullptr, nullptr) == SQLITE_OK,
           "the fake holds games");
    sqlite3_close(database);
    const auto before = fs::last_write_time(db);
    const std::vector<own::Entry> found = own::lutrisGames(dir / "lutris");
    expect(found.size() == 2 && found[0].title == "Elden Ring" && found[0].originId == "9" &&
               found[1].title == "Lies of P" && found[0].origin == own::Origin::Lutris,
           "installed games by name");
    expect(fs::last_write_time(db) == before && !fs::exists(dir / "lutris/pga.db-journal") &&
               !fs::exists(dir / "lutris/pga.db-shm"),
           "Lutris's database is only read");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "own-games"};
    recordsKeepEntries(scratch.path() / "records");
    filesAreKnownAndTitled(scratch.path() / "files");
    launchesFollowTheKind(scratch.path() / "launch");
    windowsRunsInItsOwnPrefix(scratch.path() / "windows");
    steamShortcutsAreRead(scratch.path() / "shortcuts");
    lutrisGamesAreRead(scratch.path() / "lutris");
    std::printf("own_games: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

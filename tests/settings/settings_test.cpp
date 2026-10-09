// The settings file: defaults, a round trip, and what a damaged file does.
#include "settings/settings.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "raylib.h"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::LibraryMode;
using opensu::settings::Settings;
using opensu::settings::Store;
using opensu::test::expect;

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

Settings withMode(LibraryMode mode) {
    Settings settings;
    settings.libraryMode = mode;
    return settings;
}

Settings unpinned() {
    Settings settings;
    settings.pinLibraryDock = false;
    return settings;
}

opensu::library::Game hades() {
    opensu::library::Game game;
    game.id = "steam:1";
    game.title = "Hades";
    return game;
}

void defaults(const fs::path& root) {
    const Store store{root / "none" / "settings.json"};
    expect(store.load() == Settings{}, "no file gives the defaults");
    expect(store.load().libraryMode == LibraryMode::Standard, "Library starts as Standard");
}

void roundTrip(const fs::path& root) {
    const Store store{root / "nested" / "dir" / "settings.json"};
    for (const LibraryMode mode : opensu::library::allLibraryModes) {
        std::string error;
        expect(store.save(withMode(mode), error), "a save makes its directories and writes");
        expect(store.load().libraryMode == mode, "what was saved is read back");
    }
    expect(!fs::exists(fs::path{store.file()}.concat(".part")),
           "the staging file is renamed away, not left");
    expect(read(store.file()).find("\"libraryMode\": \"carousel\"") != std::string::npos,
           "the file is JSON with the mode's key");
}

void pinnedDock(const fs::path& root) {
    const fs::path file = root / "pin" / "settings.json";
    const Store store{file};
    expect(store.load().pinLibraryDock, "the dock is pinned on Library by default");
    std::string error;
    expect(store.save(unpinned(), error), "an unpinned dock saves");
    expect(!store.load().pinLibraryDock, "and is read back unpinned");
    write(file, R"({"libraryMode": "xmb"})");
    expect(store.load().pinLibraryDock, "a file from before the option keeps the default");
    write(file, R"({"pinLibraryDock": "no"})");
    expect(store.load().pinLibraryDock, "a pin that is not a boolean keeps the default");
}

void damaged(const fs::path& root) {
    const fs::path file = root / "damaged" / "settings.json";
    const Store store{file};
    write(file, "{ not json");
    expect(store.load() == Settings{}, "text that is not JSON gives the defaults");
    write(file, "[1, 2]");
    expect(store.load() == Settings{}, "JSON that is not an object gives the defaults");
    write(file, R"({"libraryMode": "list"})");
    expect(store.load() == Settings{}, "a layout opensu has no card for gives the default");
    write(file, R"({"libraryMode": 3})");
    expect(store.load() == Settings{}, "a mode that is not a string gives the default");
    write(file, R"({"libraryMode": "xmb", "other": true})");
    expect(store.load().libraryMode == LibraryMode::Xmb, "a key opensu does not know is ignored");
}

void libraryTools(const fs::path& root) {
    const fs::path file = root / "tools" / "settings.json";
    const Store store{file};
    expect(store.load().iconSize == opensu::settings::defaultIconSize, "icon size defaults to 9");
    Settings saved;
    saved.iconSize = 15;
    saved.view.sort = opensu::library::SortKey::Name;
    saved.view.installedOnly = true;
    saved.view.source = "launcher:epic";
    saved.view.search = "not kept";
    saved.view.hiddenOnly = true;
    saved.hidden.set(hades(), true);
    saved.emulators.set("rom:/r/Kirby.nsp", "Ryujinx");
    saved.lastPlayed.record("epic:celeste", std::chrono::system_clock::from_time_t(1700000000));
    std::string error;
    expect(store.save(saved, error), "the tools save");
    const Settings loaded = store.load();
    expect(loaded.iconSize == 15 && loaded.view.sort == opensu::library::SortKey::Name &&
               loaded.view.installedOnly && loaded.view.source == "launcher:epic",
           "icon size, sort and filters round trip");
    expect(loaded.view.search.empty() && !loaded.view.hiddenOnly,
           "the search and the hidden filter are for this session only");
    expect(loaded.hidden == saved.hidden && loaded.lastPlayed == saved.lastPlayed,
           "hidden games and play history round trip");
    expect(loaded.emulators == saved.emulators, "the emulator picked for a ROM round trips");

    write(
        file,
        R"({"iconSize": 99, "sort": "bogus", "hidden": [1, "id:a"], "lastPlayed": {"x": "y", "z": 5}})");
    const Settings damagedTools = store.load();
    expect(damagedTools.iconSize == opensu::settings::defaultIconSize,
           "an icon size out of range keeps the default");
    expect(damagedTools.view.sort == opensu::library::SortKey::Recent, "an unknown sort too");
    expect(damagedTools.hidden.keys().empty(), "a hidden list with a non-string hides nothing");
    expect(damagedTools.lastPlayed.entries().empty(),
           "a play time that is not a number is dropped");
}

void unwritable(const fs::path& root) {
    const fs::path blocker = root / "blocker";
    write(blocker, "a file where a directory should be");
    const Store store{blocker / "settings.json"};
    std::string error;
    expect(!store.save(withMode(LibraryMode::Xmb), error) && !error.empty(),
           "a save that cannot happen says why");
    expect(read(blocker) == "a file where a directory should be", "and leaves what was there");
}

void installAndSources(const fs::path& root) {
    const fs::path file = root / "sources" / "settings.json";
    const Store store{file};
    const Settings fresh = store.load();
    expect(fresh.uiSounds && !fresh.homeMode && fresh.romFolders.empty() &&
               fresh.steamRoots.empty() &&
               fresh.installFolders == opensu::settings::InstallFolders{},
           "sounds are on and nothing else is set by default");
    Settings saved;
    saved.uiSounds = false;
    saved.homeMode = opensu::config::HomeMode::WiiSu;
    saved.romFolders = {"/mnt/roms", "/home/p/Roms"};
    saved.steamRoots = {"/mnt/steam"};
    saved.installFolders.setDefault("/mnt/games");
    saved.installFolders.setStore(opensu::library::Source::Gog, "/mnt/gog");
    std::string error;
    expect(store.save(saved, error), "the sources save");
    expect(store.load() == saved, "sounds, scroll mode, folders and install folders round trip");
    expect(read(file).find("\"epic\"") == std::string::npos,
           "a store that follows the default writes no folder");

    write(file, R"({"homeMode": "carousel", "romFolders": [1], "installFolders": {"steam": 4}})");
    const Settings damagedSources = store.load();
    expect(!damagedSources.homeMode && damagedSources.romFolders.empty() &&
               damagedSources.installFolders == opensu::settings::InstallFolders{},
           "damaged source settings fall back to nothing set");
}

void scaleAndShortcuts(const fs::path& root) {
    using namespace opensu::input;
    const fs::path file = root / "shortcuts" / "settings.json";
    const Store store{file};
    expect(store.load().uiScale == opensu::settings::defaultUiScale &&
               store.load().shortcuts.keys.empty(),
           "the scale is 100% and the shortcuts are the shipped ones by default");
    Settings saved;
    saved.uiScale = 125;
    Shortcuts table;
    expect(table.rebind(Action::Quit, Combo{KEY_F9}).empty(), "a remap");
    expect(table
               .rebind(Action::VolumeUp,
                       PadChord{opensu::gamepad::Button::R2, opensu::gamepad::Button::Up})
               .empty(),
           "a pad remap");
    saved.shortcuts = table.overrides();
    std::string error;
    expect(store.save(saved, error), "saved");
    const Settings loaded = store.load();
    expect(loaded == saved && loaded.shortcuts == table.overrides(),
           "scale and shortcuts round trip");

    write(
        file,
        R"({"uiScale": 400, "shortcuts": {"keys": {"quit": {"key": 81, "ctrl": false, "shift": false},
        "nothing": {"key": 65}, "volumeUp": {"key": 69}, "volumeDown": 7}, "pads": {"volumeUp": ["a", "b"], "volumeMute": ["r2"]}}})");
    const Settings damagedFile = store.load();
    expect(damagedFile.uiScale == opensu::settings::defaultUiScale,
           "an out-of-range scale is dropped");
    expect(damagedFile.shortcuts == ShortcutOverrides{},
           "an unknown action, a clash and a bad chord are each dropped");

    write(file, R"({"shortcuts": 3})");
    expect(store.load().shortcuts == ShortcutOverrides{}, "a damaged table keeps the shipped one");
}

void resolution() {
    opensu::config::Config env;
    env.homeMode = opensu::config::HomeMode::WiiSu;
    env.romRoots = {"/env/roms"};
    env.steamRoots = {"/env/steam"};
    const opensu::config::Config untouched = opensu::settings::resolved(env, Settings{});
    expect(untouched.homeMode == opensu::config::HomeMode::WiiSu &&
               untouched.romRoots == env.romRoots && untouched.steamRoots == env.steamRoots,
           "with nothing set, the environment's values stand");
    Settings chosen;
    chosen.homeMode = opensu::config::HomeMode::Standard;
    chosen.romFolders = {"/set/roms"};
    const opensu::config::Config over = opensu::settings::resolved(env, chosen);
    expect(over.homeMode == opensu::config::HomeMode::Standard &&
               over.romRoots == chosen.romFolders && over.steamRoots == env.steamRoots,
           "what the player set wins over the environment, field by field");
}

void steppedScale() {
    using opensu::settings::steppedUiScale;
    const int step = opensu::settings::uiScaleStep;
    expect(steppedUiScale(100, 1) == 100 + step && steppedUiScale(100, -1) == 100 - step,
           "a step moves one size");
    expect(steppedUiScale(100, 0) == 100 + step, "A is one step up");
    expect(steppedUiScale(opensu::settings::maxUiScale, 1) == opensu::settings::minUiScale &&
               steppedUiScale(opensu::settings::minUiScale, -1) == opensu::settings::maxUiScale,
           "the sizes wrap");
}

} // namespace

int main() {
    steppedScale();
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "settings";
    fs::remove_all(root);
    defaults(root);
    roundTrip(root);
    pinnedDock(root);
    damaged(root);
    libraryTools(root);
    installAndSources(root);
    scaleAndShortcuts(root);
    resolution();
    unwritable(root);
    fs::remove_all(root);
    std::printf("settings: all checks passed\n");
    return 0;
}

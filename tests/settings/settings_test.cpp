// The settings file: defaults, a round trip, and what a damaged file does.
#include "settings/settings.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "support/test_main.hpp"
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
    saved.home.set("console:gc", true, false);
    saved.home.set("title:hades", true, false);
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
    expect(loaded.home == saved.home && loaded.home.entries().size() == 2,
           "what the player added to Home and took off it round trips");

    write(
        file,
        R"({"iconSize": 99, "sort": "bogus", "hidden": [1, "id:a"], "lastPlayed": {"x": "y", "z": 5}, "home": {"id:a": "pinned"}})");
    const Settings damagedTools = store.load();
    expect(damagedTools.iconSize == opensu::settings::defaultIconSize,
           "an icon size out of range keeps the default");
    expect(damagedTools.view.sort == opensu::library::SortKey::Recent, "an unknown sort too");
    expect(damagedTools.hidden.keys().empty(), "a hidden list with a non-string hides nothing");
    expect(damagedTools.lastPlayed.entries().empty(),
           "a play time that is not a number is dropped");
    expect(damagedTools.home.entries().empty(), "a Home choice that is not one keeps the default");
}

void formerCopyChoices(const fs::path& root) {
    const fs::path file = root / "copies" / "settings.json";
    fs::create_directories(file.parent_path());
    write(file, R"({"iconSize": 15, "copies": {"title:hades": "epic", "title:a": 5}})");
    const Store store{file};
    const Settings loaded = store.load();
    expect(loaded.iconSize == 15, "a file with the former copy choices loads");
    std::string error;
    expect(store.save(loaded, error), "and saves");
    std::ifstream saved{file};
    const std::string text{std::istreambuf_iterator<char>{saved}, {}};
    expect(text.find("copies") == std::string::npos, "without them");
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
    saved.findRomFolders = false;
    saved.steamRoots = {"/mnt/steam"};
    saved.installFolders.setDefault("/mnt/games");
    saved.installFolders.setStore(opensu::library::Source::Gog, "/mnt/gog");
    std::string error;
    expect(store.save(saved, error), "the sources save");
    expect(store.load() == saved, "sounds, scroll mode, folders and install folders round trip");
    expect(read(file).find("\"epic\": \"") == std::string::npos,
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
    expect(table.rebind(Action::Quit, Combo{keys::F9}).empty(), "a remap");
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
        "nothing": {"key": 65}, "volumeUp": {"key": 69}, "volumeDown": 7}, "pads": {"volumeUp": ["a", "nothing"], "volumeMute": ["r2"]}}})");
    const Settings damagedFile = store.load();
    expect(damagedFile.uiScale == opensu::settings::defaultUiScale,
           "an out-of-range scale is dropped");
    expect(damagedFile.shortcuts == ShortcutOverrides{},
           "an unknown action, a clash and a bad chord are each dropped");

    write(file, R"({"shortcuts": 3})");
    expect(store.load().shortcuts == ShortcutOverrides{}, "a damaged table keeps the shipped one");
}

void wineChoices(const fs::path& root) {
    const Store store{root / "wine" / "settings.json"};
    expect(store.load().wineRunner.empty() && store.load().ownWinePrefixes.ids().empty(),
           "by default the first runner found and one shared prefix");
    Settings saved;
    saved.wineRunner = "/opt/proton/files/bin/wine";
    saved.ownWinePrefixes.set("gog:1", true);
    std::string error;
    expect(store.save(saved, error), "the Wine choices are saved");
    expect(store.load() == saved, "the runner and the games with their own prefix read back");
    write(store.file(), R"({"wineRunner": 3, "ownWinePrefix": "gog:1"})");
    expect(store.load().wineRunner.empty() && store.load().ownWinePrefixes.ids().empty(),
           "damaged Wine choices take the defaults");
}

void nightLight(const fs::path& root) {
    using opensu::settings::NightLightChoice;
    const Store store{root / "night" / "settings.json"};
    expect(store.load().nightLight == NightLightChoice::Desktop,
           "the night light follows the desktop by default");
    for (const NightLightChoice choice : opensu::settings::allNightLightChoices) {
        Settings saved;
        saved.nightLight = choice;
        std::string error;
        expect(store.save(saved, error), "the night light choice is saved");
        expect(store.load().nightLight == choice, "each night light choice reads back");
    }
    expect(read(store.file()).find("\"nightLight\": \"on\"") != std::string::npos,
           "the choice is kept by its spelling");
    write(store.file(), R"({"nightLight": "dim"})");
    expect(store.load().nightLight == NightLightChoice::Desktop,
           "an unknown night light choice follows the desktop");
}

void interfaceSounds(const fs::path& root) {
    using opensu::settings::SoundSet;
    const Store store{root / "sounds" / "settings.json"};
    expect(store.load().interfaceSounds == SoundSet::Marimba, "Marimba by default");
    Settings saved;
    saved.interfaceSounds = SoundSet::Glass;
    std::string error;
    expect(store.save(saved, error), "the sound choice is saved");
    expect(store.load().interfaceSounds == SoundSet::Glass, "Glass reads back");
    expect(read(store.file()).find("\"interfaceSounds\": \"glass\"") != std::string::npos,
           "the choice is kept by its spelling");
    saved.interfaceSounds = SoundSet::Iisu;
    expect(store.save(saved, error) && store.load().interfaceSounds == SoundSet::Iisu,
           "iiSU's sounds read back");
    write(store.file(), R"({"interfaceSounds": "opensu"})");
    expect(store.load().interfaceSounds == SoundSet::Marimba,
           "an older file's openSU sounds are Marimba");
    write(store.file(), R"({"interfaceSounds": "loud", "consoleTheme": "photos"})");
    expect(store.load().interfaceSounds == SoundSet::Marimba &&
               store.load().consoleTheme == "photos",
           "an unknown sound choice plays Marimba and leaves the console art theme alone");
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
    expect(untouched.findRomRoots, "ROM folders are looked for until the player says otherwise");
    Settings emptied;
    emptied.romFolders = {"/set/a", "/set/b"};
    emptied.removeRomFolder("/absent");
    emptied.removeRomFolder("/set/a");
    expect(emptied.romFolders == std::vector<std::filesystem::path>{"/set/b"} &&
               emptied.findRomFolders,
           "one of several folders goes; the rest stand");
    emptied.removeRomFolder("/set/b");
    expect(!opensu::settings::resolved(opensu::config::Config{}, emptied).findRomRoots,
           "the last one gone, none are looked for");
    emptied.saveShareFolder = "/share";
    emptied.installFolders.setStore(opensu::library::Source::Gog, "/gog");
    expect(emptied.namesFolder("/share") && emptied.namesFolder("/gog") &&
               !emptied.namesFolder("/set/b"),
           "a folder is named by whatever setting keeps it");
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

/// Integration choices: all off with no file, kept as chosen, a damaged entry all off.
void integrationChoices(const fs::path& root) {
    const Store store{root / "integrations" / "settings.json"};
    expect(store.load().integrations == opensu::settings::IntegrationChoices{},
           "no file has every integration off");
    Settings chosen;
    chosen.integrations =
        opensu::settings::IntegrationChoices{.steam = true,
                                             .steamMode = opensu::settings::SteamMode::Isolated,
                                             .steamPads = opensu::settings::SteamPads::All,
                                             .epic = false,
                                             .gog = true,
                                             .emulators = false,
                                             .plugins = true};
    std::string error;
    expect(store.save(chosen, error), "choices save");
    expect(store.load().integrations == chosen.integrations, "choices read back");
    expect(read(store.file()).find("\"steamMode\": \"isolated\"") != std::string::npos &&
               read(store.file()).find("\"steamPads\": \"all\"") != std::string::npos,
           "the mode and Isolated's controllers are kept by name");

    write(store.file(), R"({"integrations": "everything"})");
    expect(store.load().integrations == opensu::settings::IntegrationChoices{},
           "a damaged entry leaves every integration off");
    write(store.file(), R"({"integrations": {"steam": true, "steamMode": "sandboxed"}})");
    const auto odd = store.load().integrations;
    expect(odd.steam && !odd.steamMode, "an unknown mode takes the default");
}

void anOldInterfaceSizeKeepsItsSize(const fs::path& root) {
    using opensu::settings::uiScaleBase;
    using opensu::settings::uiScaleFactor;
    const fs::path file = root / "uiscale" / "settings.json";
    const Store store{file};
    expect(uiScaleFactor(100) == uiScaleBase && uiScaleFactor(0) == 0.0f,
           "the drawn scale is the percent over the base");
    write(file, R"({"uiScale": 80})");
    expect(store.load().uiScale == 100,
           "a size saved before the base changed is moved up to keep it");
    write(file, R"({"uiScale": 80, "uiScaleBase": 2})");
    expect(store.load().uiScale == 80, "a size written since is left alone");
}

} // namespace

int main() try {
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
    anOldInterfaceSizeKeepsItsSize(root);
    wineChoices(root);
    nightLight(root);
    interfaceSounds(root);
    formerCopyChoices(root);
    integrationChoices(root);
    resolution();
    unwritable(root);
    fs::remove_all(root);
    std::printf("settings: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

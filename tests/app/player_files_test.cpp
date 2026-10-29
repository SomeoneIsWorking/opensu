// The player's files as the shell keeps them: a hidden run's settings, game settings, mappings, own
// games and chosen covers change for the run and leave the player's files as they were; an
// ordinary run writes them in place.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "artwork/cover_overrides.hpp"
#include "config/config.hpp"
#include "fileio/atomic_write.hpp"
#include "game_settings.hpp"
#include "library/emulator_choice.hpp"
#include "library/own_games.hpp"
#include "player_files.hpp"
#include "preferences.hpp"
#include "settings/mapping_store.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;
namespace fs = std::filesystem;

std::optional<std::string> bytesOf(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, {}};
}

struct Home {
    test::ScratchDir scratch;
    fs::path config{scratch.path() / "config"};
    fs::path data{scratch.path() / "data"};
    fs::path saves{data / "saves"};
    fs::path copies{scratch.path() / "runtime" / "hidden"};
    fs::path fitted{scratch.path() / "cache" / "fitted"};

    explicit Home(const std::string& name) : scratch{OPENSU_TEST_SCRATCH, name} {
        const app::PlayerFiles inPlace{{.config = config, .data = data, .saves = saves}};
        settings::Settings values;
        values.iconSize = 7;
        std::string error;
        expect(settings::Store{inPlace.settingsFile()}.save(values, error), "settings written");
        settings::SettingLayers layers;
        layers.set(settings::Layer{settings::Layer::Kind::Global, ""}, library::emulatorKey,
                   "snes9x");
        expect(settings::GameSettingsStore{inPlace.gameSettingsFile()}.save(layers, error),
               "game settings written");
        input::MappingScopes scopes;
        scopes.setSteamInputGlobal(true);
        expect(settings::MappingStore{inPlace.mappingsFile()}.save(scopes, error),
               "mappings written");
        library::own::Records{inPlace.ownGamesFile()}.add(
            {.id = {}, .title = "Mine", .file = data / "mine"});
        expect(artwork::CoverOverrides{inPlace.chosenCoversDir(), fitted}.choose("own:1", "cover",
                                                                                 error),
               "a chosen cover kept");
        expect(fileio::writeWhole(saves / "gba" / "game.srm", "ram", error), "a save kept");
    }

    [[nodiscard]] std::vector<std::optional<std::string>> files() const {
        const app::PlayerFiles inPlace{{.config = config, .data = data, .saves = saves}};
        return {bytesOf(inPlace.settingsFile()),
                bytesOf(inPlace.gameSettingsFile()),
                bytesOf(inPlace.mappingsFile()),
                bytesOf(inPlace.ownGamesFile()),
                bytesOf(inPlace.chosenCoversDir() / "own%3A1"),
                bytesOf(inPlace.chosenCoversDir() / "steam%3A220"),
                bytesOf(saves / "gba" / "game.srm")};
    }
};

void ignore(const std::string&) {
}

void hiddenRunsKeepThePlayersFiles() {
    const Home home{"player-files-hidden"};
    const auto before = home.files();
    {
        const app::PlayerFiles files{
            {.config = home.config, .data = home.data, .saves = home.saves}, home.copies};
        app::Preferences preferences{settings::Store{files.settingsFile()}, ignore};
        expect(preferences.values().iconSize == 7, "a hidden run reads the player's settings");
        preferences.values().iconSize = 11;
        preferences.values().lastPlayed.record("own:1", std::chrono::system_clock::now());
        preferences.save();
        expect(settings::Store{files.settingsFile()}.load().iconSize == 11,
               "the run reads back what it saved");

        app::GameSettings gameSettings{settings::GameSettingsStore{files.gameSettingsFile()},
                                       files.settingsFile(), ignore};
        gameSettings.layers().set(settings::Layer{settings::Layer::Kind::Global, ""},
                                  library::emulatorKey, "bsnes");
        gameSettings.save();

        const settings::MappingStore mappings{files.mappingsFile()};
        input::MappingScopes scopes = mappings.load();
        expect(scopes.steamInputGlobal(), "a hidden run reads the player's mappings");
        scopes.setSteamInputGlobal(false);
        std::string error;
        expect(mappings.save(scopes, error) && !mappings.load().steamInputGlobal(),
               "the run reads back its mappings");

        const library::own::Records records{files.ownGamesFile()};
        records.add({.id = {}, .title = "Added", .file = home.data / "added"});
        expect(records.all().size() == 2, "the run sees the game it added");

        const artwork::CoverOverrides covers{files.chosenCoversDir(), home.fitted};
        expect(!covers.shown("own:1").empty(), "the player's chosen cover shows");
        expect(covers.choose("steam:220", "new", error) && !covers.shown("steam:220").empty(),
               "a cover chosen in the run shows");
        config::Config config;
        config.savesDir = home.saves;
        const fs::path runSaves = files.savingIn(config).savesDir / "gba" / "game.srm";
        expect(bytesOf(runSaves) == "ram", "the runner finds the player's save");
        expect(fileio::writeWhole(runSaves, "played", error), "the runner saves in the run");
        expect(fs::exists(home.copies), "the copies are kept while the run lasts");
    }
    expect(home.files() == before, "the player's files are as they were");
    expect(!fs::exists(home.copies), "the copies go with the run");
}

void ordinaryRunsWriteInPlace() {
    const Home home{"player-files-ordinary"};
    {
        const app::PlayerFiles files{
            {.config = home.config, .data = home.data, .saves = home.saves}};
        app::Preferences preferences{settings::Store{files.settingsFile()}, ignore};
        preferences.values().iconSize = 11;
        preferences.save();
    }
    expect(settings::Store{home.config / settings::settingsFile}.load().iconSize == 11,
           "an ordinary run keeps its change in the player's settings");
}

} // namespace

int main() try {
    hiddenRunsKeepThePlayersFiles();
    ordinaryRunsWriteInPlace();
    std::printf("player_files: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// The setting layers: the winning layer per key and its source, clearing back to the layer under
// it, the game-settings file round trip and damage, the one-time move of settings.json's emulator
// picks, and the per-launch video file.
#include "settings/game_settings_store.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <nlohmann/json.hpp>

#include "library/emulator_choice.hpp"
#include "settings/emulator_pick_migration.hpp"
#include "settings/video_settings.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::settings;
using opensu::library::emulatorKey;
using opensu::test::expect;
using Kind = Layer::Kind;

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void theNearestLayerWins() {
    SettingLayers layers;
    const LayerKeys chrono{"rom:/r/Chrono.smc", "snes", "snes9x-core"};
    const LayerKeys metroid{"rom:/r/Metroid.sfc", "snes", "snes9x-core"};
    const LayerKeys kirby{"rom:/r/Kirby.gba", "gba", "mgba-core"};
    expect(layers.resolve(chrono, "video.aspect", "core") == Resolved{"core", std::nullopt},
           "with nothing kept, the built-in default applies");
    layers.set(Layer{}, "video.aspect", "stretch");
    layers.set(Layer{Kind::System, "snes"}, "video.aspect", "4:3");
    expect(layers.resolve(kirby, "video.aspect", "core").from == Layer{} &&
               layers.resolve(chrono, "video.aspect", "core") ==
                   Resolved{"4:3", Layer{Kind::System, "snes"}},
           "a console's value comes before the one set everywhere");
    layers.set(Layer{Kind::Emulator, "snes9x-core"}, "video.aspect", "pixel");
    layers.set(Layer{Kind::Game, chrono.game}, "video.aspect", "stretch");
    expect(layers.resolve(chrono, "video.aspect", "core").from == Layer{Kind::Game, chrono.game} &&
               layers.resolve(metroid, "video.aspect", "core") ==
                   Resolved{"pixel", Layer{Kind::Emulator, "snes9x-core"}},
           "a game's own beats its core's, which beats its console's");
    layers.clear(Layer{Kind::Game, chrono.game}, "video.aspect");
    expect(layers.resolve(chrono, "video.aspect", "core").value == "pixel" &&
               !layers.layers().contains(Layer{Kind::Game, chrono.game}),
           "clearing deletes the key, the layer under applies, and an empty layer goes");
    expect(resolveVideo(layers, metroid).aspect == Aspect::Square &&
               resolveVideo(layers, metroid).sampling == Sampling::Nearest,
           "video settings resolve to their typed values, defaults where nothing is kept");
}

void roundTripAndDamage(const fs::path& root) {
    const GameSettingsStore store{root / "rt" / gameSettingsFile};
    expect(store.load() == SettingLayers{}, "a missing file keeps nothing");
    SettingLayers saved;
    saved.set(Layer{}, "video.sampling", "bilinear");
    saved.set(Layer{Kind::System, "snes"}, "video.aspect", "4:3");
    saved.set(Layer{Kind::Emulator, "mgba-core"}, "video.integer", "on");
    saved.set(Layer{Kind::Game, "rom:/r/Chrono.smc"}, emulatorKey, "Snes9x (built-in)");
    std::string error;
    expect(store.save(saved, error) && store.load() == saved, "every layer round trips");
    write(store.file(), R"({"global": {"video.aspect": 3}, "systems": [1],
                           "games": {"g": {"video.aspect": "pixel"}, "h": 7}})");
    SettingLayers damaged;
    damaged.set(Layer{Kind::Game, "g"}, "video.aspect", "pixel");
    expect(store.load() == damaged, "damaged entries are left out, the rest kept");
}

void picksMoveOnce(const fs::path& root) {
    const fs::path settings = root / "mig" / "settings.json";
    const GameSettingsStore store{root / "mig" / gameSettingsFile};
    write(settings, R"j({"iconSize": 12, "emulators": {"rom:/r/a.zip": "FinalBurn Neo (built-in)"},
                        "systemEmulators": {"snes": "Snes9x (built-in)", "gba": "mGBA"},
                        "integrations": {"emulators": true}})j");
    SettingLayers layers;
    layers.set(Layer{Kind::System, "gba"}, emulatorKey, "mGBA (built-in)");
    MovedPicks moved;
    std::string error;
    expect(migrateEmulatorPicks(settings, layers, store, moved, error) && moved.games == 1 &&
               moved.systems == 1,
           "the picks move, a layer's own pick kept");
    expect(store.load() == layers &&
               *layers.at(Layer{Kind::Game, "rom:/r/a.zip"}, emulatorKey) ==
                   "FinalBurn Neo (built-in)" &&
               *layers.at(Layer{Kind::System, "gba"}, emulatorKey) == "mGBA (built-in)",
           "and are saved in the game settings");
    const nlohmann::json left = nlohmann::json::parse(read(settings));
    expect(!left.contains("emulators") && !left.contains("systemEmulators") &&
               left["iconSize"] == 12 && left["integrations"]["emulators"] == true,
           "settings.json loses the old keys alone");
    MovedPicks again;
    expect(migrateEmulatorPicks(settings, layers, store, again, error) && again.games == 0 &&
               again.systems == 0,
           "a second start moves nothing");
}

void launchFile() {
    const VideoSettings video{Aspect::FourThree, IntegerScale::Overscale, Sampling::SharpBilinear};
    std::string error;
    expect(parseLaunchFile(launchFileText(video), error) == video, "the launch file round trips");
    expect(parseLaunchFile("{}", error) == VideoSettings{}, "a key it lacks keeps its default");
    expect(!parseLaunchFile(R"({"video.aspect": "wide"})", error) && !error.empty(),
           "a value that is no choice is refused");
}

} // namespace

int main() {
    try {
        const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "game_settings_store"};
        theNearestLayerWins();
        roundTripAndDamage(scratch.path());
        picksMoveOnce(scratch.path());
        launchFile();
        std::printf("game_settings_store: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}

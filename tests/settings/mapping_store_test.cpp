// The mappings file: a round trip, defaults for a missing or damaged file, damaged entries left
// out, and a save that cannot be written leaving the old file whole.
#include "settings/mapping_store.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::input;
using opensu::gamepad::Button;
using opensu::settings::Layer;
using opensu::settings::MappingStore;
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

MappingScopes sample() {
    MappingScopes scopes;
    ButtonMap global;
    expect(global.setButton(Button::A, {Button::B}).empty() &&
               global.setButton(Button::B, {Button::A}).empty() &&
               global.setButton(Button::X, {}).empty(),
           "the global swap");
    scopes.set(Layer{}, global);
    ButtonMap emulator;
    expect(emulator.setChord(PadChord{Button::Select, Button::R1}, Action::SaveState).empty() &&
               emulator
                   .setChord(PadChord{Button::Start, Button::L2},
                             PadOutput{Button::Select, Button::Start})
                   .empty(),
           "an emulator's chords");
    scopes.set(Layer{Kind::Emulator, "pcsx2"}, emulator);
    scopes.set(Layer{Kind::Game, "rom:/r/Kirby.gba"}, ButtonMap{});
    scopes.setSteamInputGlobal(true);
    scopes.setSteamInput("steam:620", false);
    return scopes;
}

void roundTrip(const fs::path& dir) {
    const MappingStore store{dir / "config" / "mappings.json"};
    expect(store.load() == MappingScopes{}, "a missing file keeps nothing");
    std::string error;
    expect(store.save(sample(), error) && error.empty(), "the mappings are written");
    expect(store.load() == sample(), "and read back as they were");
    expect(store.load().at(Layer{Kind::Game, "rom:/r/Kirby.gba"}) != nullptr,
           "a game's empty mapping is kept: it still replaces its emulator's");
    expect(read(store.file()).find("\"saveState\"") != std::string::npos,
           "actions are kept by their settings spelling");
}

void damage(const fs::path& dir) {
    const MappingStore store{dir / "damaged" / "mappings.json"};
    write(store.file(), "{ not json");
    expect(store.load() == MappingScopes{}, "an unparsable file keeps nothing");
    write(store.file(), R"({
        "global": {"buttons": {"a": ["b"], "guide": ["a"], "nonsense": ["a"], "x": "y"},
                   "chords": [{"modifier": "l1", "trigger": "r1", "action": "confirm"},
                              {"modifier": "l1", "trigger": "r2", "action": "loadState"},
                              {"modifier": "l1", "trigger": "l3"}]},
        "emulators": {"pcsx2": "not a map"},
        "games": [],
        "steamInput": {"global": "yes", "games": {"steam:1": true, "steam:2": 3}}
    })");
    const MappingScopes loaded = store.load();
    const ButtonMap* global = loaded.at(Layer{});
    expect(global != nullptr && global->buttons().size() == 1 &&
               global->output(Button::A) == PadOutput{Button::B},
           "the good button stays and the damaged ones are left out");
    expect(global->chords().size() == 1 &&
               global->chords().front().output == ChordOutput{Action::LoadState},
           "as do the damaged chords");
    expect(loaded.maps().size() == 1, "a mapping that is not one is left out");
    expect(!loaded.steamInputGlobal() && loaded.steamInputOf("steam:1") == true &&
               !loaded.steamInputOf("steam:2"),
           "and so are Steam Input values that are not true or false");
}

void aFailedSaveKeepsTheOldFile(const fs::path& dir) {
    const fs::path folder = dir / "locked";
    const MappingStore store{folder / "mappings.json"};
    std::string error;
    expect(store.save(sample(), error), "the first save is written");
    const std::string before = read(store.file());
    fs::permissions(folder, fs::perms::owner_read | fs::perms::owner_exec);
    MappingScopes changed = sample();
    changed.clear(Layer{});
    const bool saved = store.save(changed, error);
    fs::permissions(folder, fs::perms::owner_all);
    expect(!saved && !error.empty(), "a save into a folder that cannot be written fails");
    expect(read(store.file()) == before && store.load() == sample(), "and the old file is whole");
    std::size_t files = 0;
    for ([[maybe_unused]] const auto& entry : fs::directory_iterator{folder}) {
        ++files;
    }
    expect(files == 1, "no half-written file is left beside it");
}

} // namespace

int main() {
    try {
        const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "mapping_store"};
        roundTrip(scratch.path());
        damage(scratch.path());
        aFailedSaveKeepsTheOldFile(scratch.path());
        std::printf("mapping_store: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}

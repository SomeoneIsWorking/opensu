#include "retro_launch.hpp"

#include <algorithm>
#include <format>
#include <vector>

#include "fileio/atomic_write.hpp"
#include "library/emulator_choice.hpp"
#include "lucent/log.h"
#include "remote_play.hpp"
#include "retro/embedded.hpp"
#include "retro/save_paths.hpp"
#include "settings/core_option_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {

constexpr std::string_view arcadeSystem = "arcade";

const library::roms::EmulatorEntry* builtInOf(const library::Game& game,
                                              const library::roms::EmulatorCatalog& catalog) {
    const auto option =
        std::ranges::find(game.emulatorOptions, game.emulator, &library::EmulatorOption::name);
    if (option == game.emulatorOptions.end()) {
        return nullptr;
    }
    const library::roms::EmulatorEntry* entry = catalog.find(option->id);
    return entry != nullptr && entry->builtIn() ? entry : nullptr;
}

std::optional<RetroVideo> retroVideoOf(const library::Game& game,
                                       const settings::SettingLayers& layers,
                                       const library::roms::EmulatorCatalog& catalog,
                                       const std::filesystem::path& shaders,
                                       const library::roms::VisualPresets& looks) {
    if (builtInOf(game, catalog) == nullptr) {
        return std::nullopt;
    }
    const settings::LayerKeys keys = library::layerKeysOf(game);
    RetroVideo found{.video = settings::resolveVideo(layers, keys, shaders)};
    const std::string choice = settings::resolvePreset(layers, keys);
    if (const library::roms::VisualPreset* preset = looks.chosen(choice, keys.system)) {
        found.artMissing = !looks.apply(*preset, found.video, shaders);
    }
    return found;
}

namespace {

/// `--arcade`'s value for `game` on `entry`: `0:0` when its listing says nothing of the set.
std::string arcadeControls(const library::Game& game, const library::roms::EmulatorEntry& entry,
                           const library::roms::RomsetDats& romsets) {
    const std::string& set =
        game.launch.conversion ? game.launch.conversion->target : game.romsetName;
    std::string error;
    const std::optional<library::roms::RomsetDat> listing =
        romsets.load(entry, library::roms::RomsetDat::Wanted{set}, error);
    const library::roms::DatSet* found = listing ? listing->find(set) : nullptr;
    if (found == nullptr) {
        lucent::info("retro", "{}: no listing of {} on {}: {}", game.title, set, entry.name, error);
        return "0:0";
    }
    const library::roms::DatControls& controls = found->controls;
    return std::format("{}:{}{}{}", controls.buttons, controls.vertical ? "v" : "", controls.ways,
                       controls.fighter ? "f" : "");
}

} // namespace

PreparedRetro prepareRetroLaunch(library::Game& game, const settings::SettingLayers& layers,
                                 const library::roms::EmulatorCatalog& catalog,
                                 const std::filesystem::path& file,
                                 const std::filesystem::path& shaders,
                                 const library::roms::VisualPresets& looks,
                                 const std::filesystem::path& saves,
                                 const library::roms::RomsetDats& romsets, bool embedded) {
    const std::optional<RetroVideo> video = retroVideoOf(game, layers, catalog, shaders, looks);
    if (!video) {
        return {};
    }
    std::string error;
    if (!fileio::writeWhole(file, settings::videoJson(video->video), error)) {
        return PreparedRetro{.refused = std::move(error)};
    }
    std::vector<std::string> options{"--saves",
                                     retro::systemSaveFolder(saves, game.sourceId).string(),
                                     "--settings", file.string(),
                                     // The player here and every guest of remote play.
                                     "--players", std::to_string(RemotePlay::maxGuests + 1)};
    for (const auto& [key, value] :
         settings::resolveCoreOptions(layers, library::layerKeysOf(game))) {
        options.emplace_back("--option");
        options.push_back(std::format("{}={}", key, value));
    }
    if (game.sourceId == arcadeSystem) {
        options.emplace_back("--arcade");
        options.push_back(arcadeControls(game, *builtInOf(game, catalog), romsets));
    }
    if (embedded) {
        options.emplace_back(retro::embeddedArgument);
    }
    // First, so an option the catalog's command gives wins.
    game.launch.args.insert(game.launch.args.begin(), options.begin(), options.end());
    lucent::info("retro", "{} starts with {}, its saves in {}", game.title, file.string(),
                 saves.string());
    return PreparedRetro{.artMissing = video->artMissing};
}

bool hostedLaunch(const library::Game& game) {
    return std::ranges::find(game.launch.args, retro::embeddedArgument) != game.launch.args.end();
}

} // namespace opensu::app

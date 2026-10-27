#include "retro_launch.hpp"

#include <algorithm>
#include <format>
#include <vector>

#include "fileio/atomic_write.hpp"
#include "library/emulator_choice.hpp"
#include "lucent/log.h"
#include "settings/core_option_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {

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

std::optional<settings::VideoSettings> retroVideoOf(const library::Game& game,
                                                    const settings::SettingLayers& layers,
                                                    const library::roms::EmulatorCatalog& catalog,
                                                    const std::filesystem::path& presets) {
    if (builtInOf(game, catalog) == nullptr) {
        return std::nullopt;
    }
    return settings::resolveVideo(layers, library::layerKeysOf(game), presets);
}

std::string prepareRetroLaunch(library::Game& game, const settings::SettingLayers& layers,
                               const library::roms::EmulatorCatalog& catalog,
                               const std::filesystem::path& file,
                               const std::filesystem::path& presets) {
    const std::optional<settings::VideoSettings> video =
        retroVideoOf(game, layers, catalog, presets);
    if (!video) {
        return {};
    }
    std::string error;
    if (!fileio::writeWhole(file, settings::videoJson(*video), error)) {
        return error;
    }
    std::vector<std::string> options{"--settings", file.string()};
    for (const auto& [key, value] :
         settings::resolveCoreOptions(layers, library::layerKeysOf(game))) {
        options.emplace_back("--option");
        options.push_back(std::format("{}={}", key, value));
    }
    // First, so an option the catalog's command gives wins.
    game.launch.args.insert(game.launch.args.begin(), options.begin(), options.end());
    lucent::info("retro", "{} starts with {}", game.title, file.string());
    return {};
}

} // namespace opensu::app

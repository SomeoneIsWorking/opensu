#include "retro_launch.hpp"

#include <algorithm>
#include <array>

#include "fileio/atomic_write.hpp"
#include "library/emulator_choice.hpp"
#include "lucent/log.h"
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
    const std::array<std::string, 2> option{"--settings", file.string()};
    game.launch.args.insert(game.launch.args.begin(), option.begin(), option.end());
    lucent::info("retro", "{} starts with {}", game.title, file.string());
    return {};
}

} // namespace opensu::app

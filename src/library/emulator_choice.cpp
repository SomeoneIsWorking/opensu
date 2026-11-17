#include "emulator_choice.hpp"

#include <algorithm>
#include <string>

namespace opensu::library {
namespace {

using settings::Layer;

/// The option `game` lists by the name `layer` keeps, or null.
const EmulatorOption* listed(const settings::SettingLayers& layers, const Layer& layer,
                             const Game& game) {
    const std::string* name = layers.at(layer, emulatorKey);
    if (name == nullptr) {
        return nullptr;
    }
    const auto option = std::ranges::find(game.emulatorOptions, *name, &EmulatorOption::name);
    return option == game.emulatorOptions.end() ? nullptr : &*option;
}

} // namespace

EmulatorPick pickFor(const settings::SettingLayers& layers, const Game& game) {
    const EmulatorSource automatic =
        game.romset.empty() ? EmulatorSource::Search : EmulatorSource::Romset;
    const Layer own{Layer::Kind::Game, dataId(game)};
    if (layers.at(own, emulatorKey) != nullptr) {
        // A pick of an emulator no longer listed keeps what the search found.
        const EmulatorOption* option = listed(layers, own, game);
        return option != nullptr ? EmulatorPick{option, EmulatorSource::Game}
                                 : EmulatorPick{nullptr, automatic};
    }
    if (game.source != Source::Rom) {
        return EmulatorPick{nullptr, automatic};
    }
    const EmulatorOption* system = listed(layers, Layer{Layer::Kind::System, game.sourceId}, game);
    // A system's pick gives way to a core the romset matches, or waits for the romset to be
    // checked.
    if (system == nullptr || system->romset == RomsetFit::Mismatch ||
        system->romset == RomsetFit::Pending) {
        return EmulatorPick{nullptr, automatic};
    }
    return EmulatorPick{system, EmulatorSource::System};
}

namespace {

void runOn(Game& game, const EmulatorOption& option) {
    game.emulator = option.name;
    game.launch = option.launch;
    if (option.installed) {
        game.unavailable.clear();
    } else if (!option.problem.empty()) {
        game.unavailable = option.name + " cannot start: " + option.problem;
    } else {
        game.unavailable = option.name + " is not installed";
    }
}

} // namespace

void runOn(Game& game, const std::string& name) {
    const auto option = std::ranges::find(game.emulatorOptions, name, &EmulatorOption::name);
    if (option != game.emulatorOptions.end()) {
        runOn(game, *option);
    }
}

void applyEmulatorPicks(const settings::SettingLayers& layers, std::vector<Game>& games) {
    for (Game& game : games) {
        if (const EmulatorOption* option = pickFor(layers, game).option) {
            runOn(game, *option);
        }
    }
}

settings::LayerKeys layerKeysOf(const Game& game) {
    settings::LayerKeys keys{dataId(game), {}, {}, false};
    if (game.source == Source::Rom) {
        keys.system = game.sourceId;
    }
    if (const EmulatorOption* runs = runningEmulator(game)) {
        keys.emulator = runs->id;
        keys.libretro = runs->builtIn;
    }
    return keys;
}

settings::Setting emulatorPickSetting(const std::vector<std::string>& names,
                                      std::string automatic) {
    settings::Setting setting;
    setting.key = std::string{emulatorKey};
    setting.label = "Emulator";
    setting.help = "What runs its games: a built-in core or an emulator of your own";
    setting.section = "Emulator";
    for (const std::string& name : names) {
        setting.choices.push_back(settings::SettingChoice{name, name});
    }
    setting.fallbackLabel = std::move(automatic);
    setting.layers =
        settings::LayerKinds{settings::Layer::Kind::System, settings::Layer::Kind::Game};
    setting.takes = settings::Takes::NextLaunch;
    return setting;
}

} // namespace opensu::library

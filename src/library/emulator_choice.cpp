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
    const Layer own{Layer::Kind::Game, game.id};
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

void applyEmulatorPicks(const settings::SettingLayers& layers, std::vector<Game>& games) {
    for (Game& game : games) {
        const EmulatorOption* option = pickFor(layers, game).option;
        if (option == nullptr) {
            continue;
        }
        game.emulator = option->name;
        game.launch = option->launch;
        if (option->installed) {
            game.unavailable.clear();
        } else if (!option->problem.empty()) {
            game.unavailable = option->name + " cannot start: " + option->problem;
        } else {
            game.unavailable = option->name + " is not installed";
        }
    }
}

settings::LayerKeys layerKeysOf(const Game& game) {
    settings::LayerKeys keys{game.id, {}, {}};
    if (game.source == Source::Rom) {
        keys.system = game.sourceId;
    }
    const auto runs = std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    if (!game.emulator.empty() && runs != game.emulatorOptions.end()) {
        keys.emulator = runs->id;
    }
    return keys;
}

} // namespace opensu::library

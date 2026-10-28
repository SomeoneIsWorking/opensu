#include "game_settings.hpp"

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "lucent/log.h"
#include "settings/core_option_settings.hpp"
#include "settings/emulator_pick_migration.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/scaling_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {
namespace {

settings::Schema keptSchema() {
    settings::Schema schema;
    settings::declareVideo(schema);
    settings::declareFrameGen(schema);
    settings::declareScaling(schema, std::nullopt);
    settings::declareCoreOptionFamily(schema);
    schema.addOpen(std::string{library::emulatorKey});
    return schema;
}

} // namespace

std::string layerName(const settings::Layer& layer, const library::roms::EmulatorCatalog& catalog) {
    switch (layer.kind) {
    case settings::Layer::Kind::Global:
        return "Global";
    case settings::Layer::Kind::System:
        return library::roms::systemLabel(layer.id);
    case settings::Layer::Kind::Emulator: {
        const library::roms::EmulatorEntry* entry = catalog.find(layer.id);
        return entry != nullptr ? std::string{entry->shortName()} : layer.id;
    }
    case settings::Layer::Kind::Game:
        return "This game";
    }
    return layer.id;
}

std::string sourceText(const std::optional<settings::Layer>& from,
                       const library::roms::EmulatorCatalog& catalog) {
    if (!from) {
        return "Default";
    }
    if (from->kind == settings::Layer::Kind::Game) {
        return "Set for this game";
    }
    return "From " + layerName(*from, catalog);
}

GameSettings::GameSettings(settings::GameSettingsStore store,
                           const std::filesystem::path& settingsFile,
                           std::function<void(const std::string&)> onFailure)
    : kept_{keptSchema()}, store_{std::move(store)}, onFailure_{std::move(onFailure)},
      layers_{store_.load()} {
    settings::MovedPicks moved;
    std::string error;
    if (!settings::migrateEmulatorPicks(settingsFile, layers_, store_, moved, error)) {
        lucent::error("settings", "emulator picks not moved out of {}: {}", settingsFile.string(),
                      error);
    }
    const bool presets = settings::moveFrameGenModesToPresets(layers_);
    if (presets || kept_.validate(layers_, store_.file().string()) > 0) {
        save();
    }
}

std::size_t GameSettings::clear(const settings::Layer& layer) {
    const std::size_t cleared = kept_.clear(layers_, layer);
    if (cleared > 0) {
        save();
    }
    return cleared;
}

void GameSettings::save() {
    std::string error;
    if (!store_.save(layers_, error)) {
        lucent::error("settings", "{}", error);
        onFailure_("cannot keep the game settings: " + error);
    }
}

} // namespace opensu::app

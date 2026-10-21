// emulator_pick_migration — the move of the emulator picks an older `settings.json` kept
// (`emulators` per game, `systemEmulators` per system) into the game and system setting layers.
// Run once at start: the layers are saved first, then the keys are dropped from `settings.json`,
// so a crash between the two moves them again and nothing is lost.
#pragma once

#include <filesystem>
#include <string>

#include "game_settings_store.hpp"
#include "setting_layers.hpp"

namespace opensu::settings {

/// What a migration moved.
struct MovedPicks {
    std::size_t games{0};
    std::size_t systems{0};
};

/// Moves the picks `settingsFile` keeps into `layers` and saves them to `store`; a layer that
/// already keeps a pick keeps its own. Nothing is moved when the file keeps none. False with
/// `error` when a file cannot be written; `settings.json` then keeps its picks.
[[nodiscard]] bool migrateEmulatorPicks(const std::filesystem::path& settingsFile,
                                        SettingLayers& layers, const GameSettingsStore& store,
                                        MovedPicks& moved, std::string& error);

} // namespace opensu::settings

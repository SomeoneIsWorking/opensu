// emulator_choice — the `emulator` setting: which emulator a ROM runs on, kept at the game and
// system setting layers. A game's own pick comes first; then its arcade romset's verdict, which a
// system's pick gives way to when the romset does not match it (the game layer's automatic value);
// then its system's pick; then the emulator the search found. Also the layer keys a game reads its
// settings through.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"
#include "settings/setting_layers.hpp"
#include "settings/setting_schema.hpp"

namespace opensu::library {

/// The key the pick is kept under at the game and system layers.
inline constexpr std::string_view emulatorKey = "emulator";

/// Where a game's emulator comes from.
enum class EmulatorSource : std::uint8_t {
    /// The game's own pick.
    Game,
    /// The core the game's arcade romset matched.
    Romset,
    /// Its system's pick.
    System,
    /// What the search found for its system.
    Search,
};

/// A game's emulator and where it comes from. `option` is the pick when one applies; null when
/// the game keeps what the search and its romset chose.
struct EmulatorPick {
    const EmulatorOption* option{nullptr};
    EmulatorSource source{EmulatorSource::Search};
};

/// The pick that applies to `game`: a pick naming an emulator no longer listed for it is none.
[[nodiscard]] EmulatorPick pickFor(const settings::SettingLayers& layers, const Game& game);

/// Points each ROM with a pick that applies at that emulator: the game's `launch` and `emulator`
/// follow the pick, and `unavailable` says so when the pick is not installed.
void applyEmulatorPicks(const settings::SettingLayers& layers, std::vector<Game>& games);

/// The emulator pick as the schema declares it where its choices are known: `names` the emulators
/// that run the system, and the empty default (no pick) read as `automatic`. Kept at a system's
/// layer and a game's; it takes effect at the next launch.
[[nodiscard]] settings::Setting emulatorPickSetting(const std::vector<std::string>& names,
                                                    std::string automatic);

/// The layers `game` reads its settings through: its own, its emulator's, its system's.
[[nodiscard]] settings::LayerKeys layerKeysOf(const Game& game);

} // namespace opensu::library

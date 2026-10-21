// retro_launch — what a game launched on a built-in core takes besides its command: its video
// settings resolved from the setting layers, written to the launch's settings file, and
// `--settings <file>` put on its command line; the same resolution for a change while it runs. The
// runner never reads the layers itself.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "library/emulator_catalog.hpp"
#include "library/game.hpp"
#include "settings/setting_layers.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {

/// The built-in emulator `game` runs on, or null when it runs on none.
[[nodiscard]] const library::roms::EmulatorEntry*
builtInOf(const library::Game& game, const library::roms::EmulatorCatalog& catalog);

/// `game`'s video settings from the layers when it runs on a built-in core (a shader preset as its
/// path under `presets`, the shader pack); nothing otherwise.
[[nodiscard]] std::optional<settings::VideoSettings>
retroVideoOf(const library::Game& game, const settings::SettingLayers& layers,
             const library::roms::EmulatorCatalog& catalog, const std::filesystem::path& presets);

/// Readies `game`, about to start, for `opensu-retro`: its settings written to `file` (a shader
/// preset as its path under `presets`, the shader pack) and the option naming it added. A game on
/// no built-in core is left alone. Empty when done, else why the file could not be written.
[[nodiscard]] std::string prepareRetroLaunch(library::Game& game,
                                             const settings::SettingLayers& layers,
                                             const library::roms::EmulatorCatalog& catalog,
                                             const std::filesystem::path& file,
                                             const std::filesystem::path& presets);

} // namespace opensu::app

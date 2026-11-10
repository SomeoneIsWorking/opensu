// retro_launch — what a game launched on a built-in core takes besides its command: the saves
// folder of the profile it plays as (`--saves`), its video settings resolved from the setting
// layers, written to the launch's settings file, and `--settings <file>` put on its command line
// with an `--option key=value` for each core option a layer keeps; an arcade game's `--arcade`
// with what its core's romset listing says of its controls; the same video resolution for a change
// while it runs. The visual preset the layers pick is applied over the layers' video here, its
// frame art included as far as it is downloaded. The runner never reads the layers, the listings
// or the preset catalog itself.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "library/emulator_catalog.hpp"
#include "library/game.hpp"
#include "library/romset_dats.hpp"
#include "library/visual_presets.hpp"
#include "settings/setting_layers.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {

/// The built-in emulator `game` runs on, or null when it runs on none.
[[nodiscard]] const library::roms::EmulatorEntry*
builtInOf(const library::Game& game, const library::roms::EmulatorCatalog& catalog);

/// A game's video settings for the runner.
struct RetroVideo {
    settings::VideoSettings video;
    /// The visual preset in force frames with art that is not downloaded yet.
    bool artMissing{false};
};

/// `game`'s video settings from the layers when it runs on a built-in core (a shader preset as its
/// path under `shaders`, the shader pack), with the visual preset they pick from `looks` applied;
/// nothing otherwise.
[[nodiscard]] std::optional<RetroVideo> retroVideoOf(const library::Game& game,
                                                     const settings::SettingLayers& layers,
                                                     const library::roms::EmulatorCatalog& catalog,
                                                     const std::filesystem::path& shaders,
                                                     const library::roms::VisualPresets& looks);

/// What readying a launch came to: why the settings file could not be written (empty when it
/// was), and whether the visual preset's art is missing.
struct PreparedRetro {
    std::string refused;
    bool artMissing{false};
};

/// Readies `game`, about to start, for `opensu-retro`: its saves in its console's folder under
/// `saves` (the profile's, `retro::profileSaveFolder`), its settings written to `file` (a shader
/// preset as its path under `shaders`, the shader pack; the visual preset from `looks`) and the
/// options naming both added, an arcade game's controls read from `romsets`, and `--embedded`
/// when `embedded`. A game on no built-in core is left alone.
[[nodiscard]] PreparedRetro
prepareRetroLaunch(library::Game& game, const settings::SettingLayers& layers,
                   const library::roms::EmulatorCatalog& catalog, const std::filesystem::path& file,
                   const std::filesystem::path& shaders, const library::roms::VisualPresets& looks,
                   const std::filesystem::path& saves, const library::roms::RomsetDats& romsets,
                   bool embedded);

/// Whether `game`'s command runs it inside the shell's window.
[[nodiscard]] bool hostedLaunch(const library::Game& game);

} // namespace opensu::app

// emulator_launch_config — the one owner of what a standalone emulator's own settings say for a
// launch openSU starts. Each emulator with a rule has the files it names set for the run through
// `LaunchFiles` and put back when the run ends; a crashed run's leftovers are put back first.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "emulator_catalog.hpp"
#include "game.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// What a launch's emulator rule works from.
struct LaunchSetup {
    /// The catalog id of the emulator the game runs on.
    std::string emulatorId;
    /// Where the install it runs as keeps its settings.
    std::filesystem::path config;
    /// The SDL GUID of the virtual pads the game reads; empty when it reads the pads themselves.
    std::string padGuid;
};

/// What `game`'s launch sets up when it runs on an emulator with a rule; nothing otherwise.
[[nodiscard]] std::optional<LaunchSetup>
launchSetupOf(const Game& game, const roms::EmulatorCatalog& catalog, std::string_view padGuid);

class EmulatorLaunchConfig {
  public:
    /// Puts back what a crashed run left, then sets the emulator's files for this run.
    explicit EmulatorLaunchConfig(const LaunchSetup& setup);

  private:
    std::optional<LaunchFiles> files_;
};

} // namespace opensu::library

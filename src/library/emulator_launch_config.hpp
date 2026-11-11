// emulator_launch_config — the one owner of what a standalone emulator's own settings say for a
// launch openSU starts. Each emulator with a rule has its settings applied to the files it names,
// written in place before the run and left there; every other setting in them stays the player's.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "emulator_catalog.hpp"
#include "game.hpp"
#include "wii_controls.hpp"

namespace opensu::library {

/// What a launch's emulator rule works from.
struct LaunchSetup {
    /// The catalog id of the emulator the game runs on.
    std::string emulatorId;
    /// Where the install it runs as keeps its settings.
    std::filesystem::path config;
    /// Where it keeps its data, when that is apart from its settings; empty otherwise.
    std::filesystem::path data;
    /// The SDL GUID of the virtual pads the game reads; empty when it reads the pads themselves.
    std::string padGuid;
    /// The emulated Wii Remote a Wii game is played with; none for another game.
    std::optional<WiiRemote> wiiRemote{};
};

/// The name SDL gives the virtual pads, from its own table for 045e:028e, which an emulator keying
/// pads by SDL name binds.
inline constexpr std::string_view virtualPadSdlName{"Xbox 360 Controller"};

/// What `game`'s launch sets up when it runs on an emulator with a rule, a Wii game's remote from
/// `wii` by its disc id; nothing otherwise.
[[nodiscard]] std::optional<LaunchSetup> launchSetupOf(const Game& game,
                                                       const roms::EmulatorCatalog& catalog,
                                                       const WiiControls& wii,
                                                       std::string_view padGuid);

/// A settings file a rule writes, and the bytes it writes there.
struct SettingsWrite {
    std::filesystem::path path;
    std::string bytes;
};

/// What `file` becomes: `settings` of what it says now, or of none when it is missing; nothing
/// when that leaves it as it is, or when it cannot be read.
[[nodiscard]] std::optional<SettingsWrite>
settingsWriteOf(const std::filesystem::path& file,
                const std::function<std::string(std::string_view)>& settings);

/// Writes what `setup`'s emulator rule sets into its files, each replaced whole. One that cannot
/// be written is logged and stays as it was.
void writeLaunchSettings(const LaunchSetup& setup);

} // namespace opensu::library

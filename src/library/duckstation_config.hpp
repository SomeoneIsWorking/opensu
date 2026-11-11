// duckstation_config — DuckStation's launch rule: `settings.ini` set for a launch openSU starts.
// Its start-up update check, whose dialog covers a fullscreen boot, its setup wizard and its
// launcher-shortcut question, which an AppImage asks before the boot, are off.
// DuckStation binds a pad as `SDL-<device>/<input>`, the device an SDL player index or a
// persistent identifier, and a launch lets SDL see only the virtual pads: `[PadN]`'s SDL bindings
// are made `SDL-<N-1>`, their inputs kept, and a standard controller bound only to the keyboard
// or to nothing gets DuckStation's own SDL layout on `SDL-<N-1>`. Input profiles under
// `inputprofiles/` are bound the same way.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"

namespace opensu::library {

/// The catalog's DuckStation.
inline constexpr std::string_view duckstationId{"duckstation"};
/// Its settings file under its data folder.
inline constexpr std::string_view duckstationSettingsFile{"settings.ini"};
/// Its input profiles' folder under its data folder.
inline constexpr std::string_view duckstationProfilesFolder{"inputprofiles"};
/// The pads DuckStation's settings bind, `[Pad1]` to `[Pad8]` with both multitaps.
inline constexpr int duckstationPads{8};

/// The pads of a settings file or input profile `ini` bound to the virtual pads.
[[nodiscard]] std::string duckstationPadSettings(std::string_view ini);

/// The settings file `ini` as a launch reads it, the update check, setup wizard and
/// launcher-shortcut question off; with `virtualPads` false the pads stay the player's.
[[nodiscard]] std::string duckstationLaunchSettings(std::string_view ini, bool virtualPads);

/// The settings file, made when the player has none, and each input profile as the launch reads
/// them.
[[nodiscard]] std::vector<SettingsWrite> duckstationWrites(const LaunchSetup& setup);

} // namespace opensu::library

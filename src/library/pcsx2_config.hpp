// pcsx2_config — PCSX2's launch rule: `inis/PCSX2.ini` set for a launch openSU starts. Its setup
// wizard is off, since it opens on a config without `[UI] SetupWizardIncomplete = false` and takes
// a mouse to finish. PCSX2 binds a pad as `SDL-<n>/<input>`, n the SDL player index, which counts
// only the virtual pads a launch lets SDL see: `[PadN]`'s SDL bindings are made `SDL-<N-1>`, their
// inputs kept, and a DualShock 2 bound only to the keyboard or to nothing gets PCSX2's own SDL
// layout on `SDL-<N-1>`. Input profiles under `inputprofiles/` are bound the same way.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"

namespace opensu::library {

/// The catalog's PCSX2.
inline constexpr std::string_view pcsx2Id{"pcsx2"};
/// Its settings file under its settings folder.
inline constexpr std::string_view pcsx2SettingsFile{"inis/PCSX2.ini"};
/// Its input profiles' folder under its settings folder.
inline constexpr std::string_view pcsx2ProfilesFolder{"inputprofiles"};
/// The pads PCSX2's settings bind, `[Pad1]` to `[Pad8]` with both multitaps.
inline constexpr int pcsx2Pads{8};

/// The pads of a settings file or input profile `ini` bound to the virtual pads.
[[nodiscard]] std::string pcsx2PadSettings(std::string_view ini);

/// The settings file `ini` as a launch reads it, the setup wizard off; with `virtualPads` false
/// the pads stay the player's.
[[nodiscard]] std::string pcsx2LaunchSettings(std::string_view ini, bool virtualPads);

/// The settings file, made when the player has none, and each input profile as the launch reads
/// them.
[[nodiscard]] std::vector<SettingsWrite> pcsx2Writes(const LaunchSetup& setup);

} // namespace opensu::library

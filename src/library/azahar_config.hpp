// azahar_config — Azahar's launch rule: `qt-config.ini` set for a launch openSU starts. Its update
// check on start is off, since the dialog it opens takes a mouse or a keyboard to close. The
// active input profile's SDL bindings, `engine:sdl,guid:<GUID>,port:<k>,...`, are made the first
// virtual pad's: Azahar keys a pad by SDL's full GUID and numbers pads of one GUID by the order
// they attach. A profile without SDL bindings gets the standard layout on the first virtual pad,
// read through SDL's game controller API as Azahar's own automap writes it.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's Azahar.
inline constexpr std::string_view azaharId{"azahar"};
/// Its settings file in its settings folder.
inline constexpr std::string_view azaharSettingsFile{"qt-config.ini"};

/// The settings file `ini` as a launch reads it with the virtual pads of `padGuid`, the update
/// check off; with `padGuid` empty only the update check is changed.
[[nodiscard]] std::string azaharLaunchSettings(std::string_view ini, std::string_view padGuid);

/// The files the rule may set under the settings folder.
[[nodiscard]] std::vector<std::filesystem::path> azaharFiles(const LaunchSetup& setup);

/// `qt-config.ini` as the launch reads it; made when the player has none.
[[nodiscard]] std::vector<RunFile> azaharRunFiles(const LaunchSetup& setup);

} // namespace opensu::library

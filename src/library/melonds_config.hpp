// melonds_config — melonDS's launch rule: `melonDS.toml` set for a launch openSU starts. melonDS
// reads one SDL joystick by index, `[Instance0] JoystickID`, which counts only the virtual pads a
// launch lets SDL see, so the first instance is given joystick 0. Its buttons are raw SDL joystick
// codes in `[Instance0.Joystick]`: the player's are kept, and when every button is unbound the
// virtual pad's are written in the DS's positional layout, the D-pad also on the left stick.
// melonDS shows no start-up prompt.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's melonDS.
inline constexpr std::string_view melondsId{"melonds"};
/// Its settings file in its settings folder.
inline constexpr std::string_view melondsSettingsFile{"melonDS.toml"};

/// The settings file `toml` as a launch on the virtual pads reads it.
[[nodiscard]] std::string melondsLaunchSettings(std::string_view toml);

/// The files the rule may set under `setup.config`.
[[nodiscard]] std::vector<std::filesystem::path> melondsFiles(const LaunchSetup& setup);

/// `melonDS.toml` as the launch reads it, made when the player has none; nothing when the game
/// reads the pads themselves.
[[nodiscard]] std::vector<RunFile> melondsRunFiles(const LaunchSetup& setup);

} // namespace opensu::library

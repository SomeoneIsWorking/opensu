// mgba_config — mGBA's launch rule: `config.ini` set for a launch openSU starts so the virtual
// pads' buttons play. mGBA's Qt frontend loads the input profile named for the pad's SDL joystick
// name,
// `[gba.input-profile.<name>]` (`InputController::profileForType`), over its `[gba.input.SDLB]`
// bindings, which bind only the stick and hat when the player has none. Its built-in profiles
// match xpad's "Microsoft X-Box 360 pad", not the "Xbox 360 Controller" SDL names the virtual pads,
// so each button the player's profile for that name lacks is added from mGBA's own xpad profile.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"

namespace opensu::library {

/// The catalog's standalone mGBA.
inline constexpr std::string_view mgbaId{"mgba"};
/// Its settings file in its settings folder.
inline constexpr std::string_view mgbaSettingsFile{"config.ini"};

/// The settings file `ini` with the virtual pads' input profile completed.
[[nodiscard]] std::string mgbaLaunchSettings(std::string_view ini);

/// `config.ini` as the launch reads it; none when the game reads the pads themselves.
[[nodiscard]] std::vector<SettingsWrite> mgbaWrites(const LaunchSetup& setup);

} // namespace opensu::library

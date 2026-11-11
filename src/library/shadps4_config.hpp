// shadps4_config — shadPS4's launch rule: `users.json` set for a launch openSU starts. shadPS4
// gives the connected pads slots 1 to 4 in SDL's order, each slot only when a user has that
// `player_index` (`GameControllers::TryOpenSDLControllers`), and reads the player's user list
// instead of its four default users when the file lists any, so a player with fewer users is given
// shadPS4's default user for each slot that has none. A player without the file gets shadPS4's own
// four. Its Qt launcher's update, compatibility and changelog checks run only in its main window,
// which a launch with a game and an emulator (`-e default -g`) never opens, and are off by default.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"

namespace opensu::library {

/// The catalog's shadPS4.
inline constexpr std::string_view shadps4Id{"shadps4"};
/// Its user list under its data folder.
inline constexpr std::string_view shadps4UsersFile{"users.json"};
/// The pad slots shadPS4 gives users.
inline constexpr int shadps4Slots{4};

/// The user list `json` with a user for every pad slot; the player's bytes when it has one for
/// each, or when it cannot be read.
[[nodiscard]] std::string shadps4LaunchUsers(std::string_view json);

/// The user list as the launch reads it; nothing when the player has none.
[[nodiscard]] std::vector<SettingsWrite> shadps4Writes(const LaunchSetup& setup);

} // namespace opensu::library

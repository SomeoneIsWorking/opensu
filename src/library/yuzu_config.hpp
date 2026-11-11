// yuzu_config — the launch rule of yuzu's line (Eden, Citron, sudachi): `qt-config.ini` set for a
// launch openSU starts. Its update check is off, since the dialog it opens takes a mouse or a
// keyboard to close. Each player's SDL bindings, `engine:sdl,guid:<GUID>,port:<N>,...`, are made
// the Nth virtual pad's, its GUID without the name's CRC as Eden keys pads: Eden numbers pads of
// one GUID by the order they attach, so the Nth pad is port N. A player without SDL bindings,
// player 1 or one connected, gets a Pro Controller in the standard layout on its virtual pad, read
// as openSU's virtual pad reports (xpad's buttons, hat and axes), positional as Nintendo's (Switch
// A east). The handheld, `player_8_`, is player 1's hands: it plays the first virtual pad, and
// while it is connected it, not player 1, gets the standard layout.
#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"

namespace opensu::library {

/// The catalog's emulators of yuzu's line.
inline constexpr std::array<std::string_view, 3> yuzuLineIds{"eden", "citron", "sudachi"};
/// Their settings file in their settings folder.
inline constexpr std::string_view yuzuSettingsFile{"qt-config.ini"};
/// The players Eden's settings bind.
inline constexpr int yuzuPlayers{8};
/// The handheld's index in Eden's settings, `NpadIdTypeToIndex(Handheld)`.
inline constexpr int yuzuHandheld{8};

/// `sdlGuid` as Eden keys a pad by: SDL's GUID with the name's CRC-16 (bytes 2 and 3) zeroed.
[[nodiscard]] std::string yuzuGuid(std::string_view sdlGuid);

/// The settings file `ini` as a launch reads it with the virtual pads of `padGuid`, the update
/// check off; with `padGuid` empty only the update check is changed.
[[nodiscard]] std::string yuzuLaunchSettings(std::string_view ini, std::string_view padGuid);

/// `qt-config.ini` under the settings folder as the launch reads it; made when the player has
/// none.
[[nodiscard]] std::vector<SettingsWrite> yuzuWrites(const LaunchSetup& setup);

} // namespace opensu::library

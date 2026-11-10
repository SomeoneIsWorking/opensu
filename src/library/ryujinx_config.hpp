// ryujinx_config — Ryujinx's launch rule: `Config.json` set for a launch openSU starts. Its update
// check is off, since the prompt it opens takes a mouse or a keyboard to close. Ryujinx binds a
// player to an SDL pad by `id`, `<k>-<GUID>`: the SDL GUID as a .NET Guid with the name's CRC
// zeroed, `k` counting connected pads of that GUID, so player N's gamepad entry is given the Nth
// virtual pad, its mappings kept. A player on the keyboard, or player 1 without an entry, gets a
// Pro Controller in the standard layout on its virtual pad. SDL3 builds' enum names are written as
// their SDL2 equivalents, which 1.3.3 reads and SDL3 builds still accept.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_launch_config.hpp"
#include "launch_files.hpp"

namespace opensu::library {

/// The catalog's Ryujinx.
inline constexpr std::string_view ryujinxId{"ryujinx"};
/// Its settings file in its settings folder.
inline constexpr std::string_view ryujinxSettingsFile{"Config.json"};
/// The players Ryujinx binds, `Player1` to `Player8`.
inline constexpr int ryujinxPlayers{8};

/// The id Ryujinx gives the `index`th connected pad of SDL GUID `sdlGuid`.
[[nodiscard]] std::string ryujinxPadId(std::string_view sdlGuid, int index);

/// The settings `json` as a launch reads them with the virtual pads of `padGuid`, the update check
/// off; with `padGuid` empty only the update check is changed. `json` as it was when it is not
/// Ryujinx's settings.
[[nodiscard]] std::string ryujinxLaunchSettings(std::string_view json, std::string_view padGuid);

/// The files the rule may set under the settings folder.
[[nodiscard]] std::vector<std::filesystem::path> ryujinxFiles(const LaunchSetup& setup);

/// `Config.json` as the launch reads it; nothing when the player has none, since Ryujinx writes
/// its own on its first start.
[[nodiscard]] std::vector<RunFile> ryujinxRunFiles(const LaunchSetup& setup);

} // namespace opensu::library

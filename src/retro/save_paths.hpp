// save_paths — where the built-in runner keeps a game's save RAM and state slot: the one owner of
// those names, which the runner reads and writes and the shell lists, copies and backs up. The
// shell keeps each player profile's saves in a folder of its own.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace opensu::retro {

/// Under the saves folder: one folder per profile.
inline constexpr std::string_view profilesFolder = "profiles";

/// Where the shell keeps `profile`'s saves: `<saves>/profiles/<profile id>`.
[[nodiscard]] std::filesystem::path profileSaveFolder(const std::filesystem::path& saves,
                                                      std::string_view profile);

/// The folder a launch from the shell saves in: `<profile folder>/<system>`.
[[nodiscard]] std::filesystem::path systemSaveFolder(const std::filesystem::path& saves,
                                                     std::string_view system);

/// The folder a launch that names no saves folder uses: `<saves>/<core name>`.
[[nodiscard]] std::filesystem::path coreSaveFolder(const std::filesystem::path& saves,
                                                   const std::filesystem::path& core);

/// A core's name without the buildbot's suffix: "mgba" for mgba_libretro.so or
/// mgba_libretro_android.so.
[[nodiscard]] std::string coreName(const std::filesystem::path& core);

/// The game's save RAM: `<folder>/<rom stem>.srm`.
[[nodiscard]] std::filesystem::path saveMemoryFile(const std::filesystem::path& folder,
                                                   const std::filesystem::path& rom);

/// The game's state slot: `<folder>/<rom stem>.state`.
[[nodiscard]] std::filesystem::path saveStateFile(const std::filesystem::path& folder,
                                                  const std::filesystem::path& rom);

} // namespace opensu::retro

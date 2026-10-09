// rom_systems — the consoles opensu knows: what a system's folder may be called, which files
// are its games, and which file in a game's folder is the one to start.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "name_list.hpp"

namespace opensu::library::roms {

struct RomSystem {
    /// ES-DE's system name, which is also the platform key for the tile's frame ("ps2").
    std::string_view key;
    std::string_view label;
    /// The system's folder at thumbnails.libretro.com, empty when it has none.
    std::string_view libretroName;
    /// Folder names, compared lower-case with everything but letters and digits removed.
    NameList folders;
    /// Lower-case extensions, dot included, of the files that are games.
    NameList extensions;
    /// A game folder's file to start when no file of `extensions` names it: a path relative to
    /// the folder, matched case-insensitively, with `*` standing for one name ("code/*.rpx").
    NameList markers;
};

/// Every known system.
[[nodiscard]] std::span<const RomSystem> romSystems();

/// The system a ROM root's subfolder holds, or null.
[[nodiscard]] const RomSystem* systemForFolder(std::string_view folderName);

/// The system with ES-DE name `key`, or null.
[[nodiscard]] const RomSystem* systemByKey(std::string_view key);

/// The file to start for one game: `entry` itself when it is a file of the system's, or the
/// file inside a game folder. In a folder a marker wins; otherwise the game file that is no
/// update or DLC, the largest when several are. Nothing when the entry holds no game.
[[nodiscard]] std::optional<std::filesystem::path> gameFile(const RomSystem& system,
                                                            const std::filesystem::path& entry);

} // namespace opensu::library::roms

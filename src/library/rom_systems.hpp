// rom_systems — the consoles opensu knows: what a system's folder may be called, which files
// are its games, which file in a game's folder is the one to start, and what a ROM root holds at
// its top: the system folders and the loose files an extension of one console's alone places.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "name_list.hpp"
#include "rom_tree.hpp"

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
    /// The extensions no other console's games use, so a loose file in a ROM root with one is
    /// this system's game; empty where every extension is shared (.iso, .chd, .cue, .zip).
    NameList loose;
};

/// A system's folder, or a loose game file, at the top of a ROM root.
struct RootEntry {
    const RomSystem* system;
    RomNode node;
};

/// What a ROM root holds at its top.
struct RootTop {
    /// The root's own entries, sorted by name.
    std::vector<RomNode> children;
    /// The folders named for a system.
    std::vector<RootEntry> folders;
    /// The files placed by an extension of one system's alone.
    std::vector<RootEntry> loose;
    /// The files with a game extension that more than one system shares, which are not guessed.
    std::size_t leftOut{0};
};

/// Every known system.
[[nodiscard]] std::span<const RomSystem> romSystems();

/// The system a ROM root's subfolder holds, or null.
[[nodiscard]] const RomSystem* systemForFolder(std::string_view folderName);

/// The system a loose file named `fileName` belongs to by its extension, or null when no system
/// owns that extension alone.
[[nodiscard]] const RomSystem* systemForFile(std::string_view fileName);

/// What `root` holds at its top; nothing when it is no folder the tree can read.
[[nodiscard]] std::optional<RootTop> readRoot(const RomTree& tree,
                                              const std::filesystem::path& root);

/// Whether `top` holds a game the scan lists: stops at the first system folder that has one.
[[nodiscard]] bool holdsGames(const RomTree& tree, const RootTop& top);

/// The system with ES-DE name `key`, or null.
[[nodiscard]] const RomSystem* systemByKey(std::string_view key);
/// The system's label, or `key` itself for a system not listed.
[[nodiscard]] std::string systemLabel(std::string_view key);

/// The file to start for one game: `entry` itself when it is a file of the system's, or the
/// file inside a game folder. In a folder a marker wins; otherwise the game file that is no
/// update or DLC (by its name, version tag, or a Switch title id not ending in 000), the largest
/// when several are. Nothing when the entry holds no game.
[[nodiscard]] std::optional<std::filesystem::path>
gameFile(const RomTree& tree, const RomSystem& system, const RomNode& entry);

} // namespace opensu::library::roms

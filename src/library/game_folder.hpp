// game_folder — the player's games folders: what a folder holds as games, read again on every
// library reload, so a file that goes takes its game with it. Windows: the `.lnk` and `.url`
// shortcuts in it and one `.exe` of each immediate subfolder. Linux: its `.desktop` entries and
// programs, and one program of each immediate subfolder, a Windows `.exe` among them (run under
// Wine). macOS: its `.app` bundles and those one subfolder down. A game is named by its shortcut,
// else by the clues around its file (`identify::cluesOf`). Nothing is kept: an entry's id comes
// from the path it was found at, so hiding it, its last played and its cover outlive the scan.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "own_games.hpp"

namespace opensu::library::folder {

/// Whose files a folder is read for.
enum class Host : std::uint8_t { Windows, Linux, Mac };

/// The OS this build runs on.
[[nodiscard]] Host thisHost() noexcept;

/// Whether `name`, a shortcut's or a program's, is not a game: an installer or uninstaller, a
/// redistributable, a crash reporter, an updater, anti-cheat, or a store client that only starts
/// other games. A word that is also a game's ("Crash Bandicoot") does not count alone.
[[nodiscard]] bool notAGame(std::string_view name);

/// Whether `file` is a shortcut, `.lnk`, `.url` or `.desktop`, as `shortcutEntry` reads them.
[[nodiscard]] bool isShortcut(const std::filesystem::path& file);

/// The game shortcut `file` starts; nothing when it starts no game or what it names is not there.
[[nodiscard]] std::optional<own::Entry> shortcutEntry(const std::filesystem::path& file);

/// Where Flatpak exports its apps' `.desktop` entries and icons: the player's installs under
/// `xdgDataHome`, then the system's.
[[nodiscard]] std::vector<std::filesystem::path>
flatpakExports(const std::filesystem::path& xdgDataHome);

/// The installed Flatpaks that are games, from the `applications` of `exports`: an entry in the
/// Game category and not the Emulator one that is no store client or launcher (`notAGame`), each
/// with origin `Folder` and started as its entry says; an app installed twice, as the first export
/// lists it.
[[nodiscard]] std::vector<own::Entry>
flatpakGames(const std::vector<std::filesystem::path>& exports);

/// The games in `folder`, each with origin `Folder`; none when it cannot be read.
[[nodiscard]] std::vector<own::Entry> scan(const std::filesystem::path& folder,
                                           Host host = thisHost());

} // namespace opensu::library::folder

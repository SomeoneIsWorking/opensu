// game_icon — the icon a game's own files carry, for a tile with no cover, and an installed
// emulator's, for the mark of what runs a ROM: from a Windows program's resources (the largest
// image of its icon group), an `.ico`, a macOS `.icns`, a picture file or a theme icon.
// Each is read from untrusted bytes, every read bounds-checked; what comes out is PNG.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "library/game.hpp"

namespace opensu::artwork {

/// The largest image of an `.ico` file, as PNG.
[[nodiscard]] std::optional<std::string> iconOfIco(std::span<const std::uint8_t> bytes);
/// The largest image of an `.icns` file stored as PNG, as PNG.
[[nodiscard]] std::optional<std::string> iconOfIcns(std::span<const std::uint8_t> bytes);
/// The largest image of icon group `index` of the program `file`: the n-th, or minus the resource
/// id when negative, the first when it has no such group.
[[nodiscard]] std::optional<std::string> iconOfProgram(const std::filesystem::path& file,
                                                       std::int32_t index);

/// `game`'s own icon: from `game.iconSource` (a file, or a theme icon's name searched in
/// `themes`, each an `icons`-like folder), else its program, else an `.ico` beside it.
[[nodiscard]] std::optional<std::string> gameIcon(const library::Game& game,
                                                  const std::vector<std::filesystem::path>& themes);

/// The icon of installed emulator `option`: a macOS `.app`'s own when its program is in one; else
/// the one the `.desktop` entry in `applications` that
/// starts it names (by its program, or its Flatpak's app id), else a theme icon in `themes` named
/// as its Flatpak or its program, else its Windows program's own. Not its catalog id, which other
/// programs share ("dolphin"). Nothing for an AppImage no entry names, whose icon is inside it.
[[nodiscard]] std::optional<std::string>
emulatorIcon(const library::EmulatorOption& option,
             const std::vector<std::filesystem::path>& themes,
             const std::vector<std::filesystem::path>& applications);

} // namespace opensu::artwork

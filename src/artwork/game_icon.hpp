// game_icon — the icon a game's own files carry, for a tile with no cover: from a Windows program's
// resources (the largest image of its icon group), an `.ico`, a macOS `.icns`, or a picture file.
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

} // namespace opensu::artwork

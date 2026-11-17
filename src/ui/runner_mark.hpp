// runner_mark — the one mark a game's tile carries for what runs its default version: its store;
// Linux, AppImage or Wine for one of the player's own; openSU's runner for a ROM it plays itself;
// another emulator's own icon, or its initials until that icon is found.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "library/game.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// What a tile's runner badge shows, first present first: `icon`, `picture`, `letters`.
struct RunnerMark {
    /// A shipped mark.
    std::optional<Icon> icon;
    /// An emulator's own icon, kept by the artwork store.
    std::filesystem::path picture;
    /// An emulator's initials.
    std::string letters;
};

/// The mark of what runs `game`; nothing for a game on another openSU.
[[nodiscard]] std::optional<RunnerMark> runnerMark(const library::Game& game);

/// Up to two letters for `name`: its words' initials ("Dolphin Emu" "DE"), else its capitals
/// ("DuckStation" "DS"), else its first two letters ("Ryujinx" "Ry").
[[nodiscard]] std::string initials(std::string_view name);

} // namespace opensu::ui

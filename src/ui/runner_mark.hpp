// runner_mark — the one mark a game's tile carries for what runs its default version: its store;
// Linux, AppImage or Wine for one of the player's own; openSU's runner for a ROM it plays itself;
// another emulator's own icon, or its initials until that icon is found; on another openSU's
// game, ringed. Also the words that say a tile's game is on another openSU too.
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
    /// It runs on another openSU, which the badge's ring says.
    bool remote{false};
};

/// The mark of what runs `game`; for a game on another openSU, what runs it there as its listing
/// says, ringed.
[[nodiscard]] std::optional<RunnerMark> runnerMark(const library::Game& game);

/// Where a tile's game is besides here, after its title: "On Desk" for a game only another
/// openSU holds, "Also on Desk" for one that is here too; empty for neither.
[[nodiscard]] std::string whereElse(const library::Game& game);

/// Up to two letters for `name`: its words' initials ("Dolphin Emu" "DE"), else its capitals
/// ("DuckStation" "DS"), else its first two letters ("Ryujinx" "Ry"); a note in brackets after
/// it is left out.
[[nodiscard]] std::string initials(std::string_view name);

} // namespace opensu::ui

// linked_play — a game played by two: on the link core, two players on this machine or this
// machine's player in a netplay session; on any other core of the built-in runner, one console on
// each machine with a player on each port. The single-play emulator pick is left alone.
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "game.hpp"

namespace opensu::library {

/// The catalog's id of the core that links players.
inline constexpr std::string_view linkCoreId = "mgba-link-core";

/// The revision of the link core this openSU ships, which every machine of a session must run.
[[nodiscard]] std::string_view linkCoreRevision() noexcept;
/// The BIOS the link core boots, in the runner's system folder, when it is there.
inline constexpr std::string_view linkCoreBios = "gba_bios.bin";

/// Whether `game` can run on the link core here.
[[nodiscard]] bool linkable(const Game& game);

/// Whether `game` can be a netplay session here, on the link core or its own built-in core.
[[nodiscard]] bool netplayable(const Game& game);

/// The library of the built-in core `game` runs on when it is not `linkable`; empty for none.
[[nodiscard]] std::filesystem::path netplayCore(const Game& game);

/// `game` on the link core, both players side by side; nothing when it is not `linkable`.
[[nodiscard]] std::optional<Game> splitScreenGame(const Game& game);

/// `game` as the machine `session` (`opensu-retro --netplay`) describes: on the link core showing
/// only its own player, or else on its own built-in core; nothing when it is not `netplayable`.
[[nodiscard]] std::optional<Game> netplayGame(const Game& game,
                                              const std::filesystem::path& session);

} // namespace opensu::library

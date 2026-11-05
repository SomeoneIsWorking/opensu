// linked_play — a game started on the link core: two players on this machine, or this machine's
// player in a netplay session. The single-play emulator pick is left alone.
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

/// `game` on the link core, both players side by side; nothing when it is not `linkable`.
[[nodiscard]] std::optional<Game> splitScreenGame(const Game& game);

/// `game` on the link core as the machine `session` (`opensu-retro --netplay`) describes, showing
/// only its own player; nothing when it is not `linkable`.
[[nodiscard]] std::optional<Game> netplayGame(const Game& game,
                                              const std::filesystem::path& session);

} // namespace opensu::library

// add_game_offer — what the player can add as a game of their own on this OS, as the Add a game
// tile says it: a file, Steam's shortcuts and Lutris games on Linux (`add_game_offer_linux.cpp`);
// no Lutris on Windows or macOS (`add_game_offer_desktop.cpp`); nothing on Android, where no
// program of the player's own can start (`add_game_offer_none.cpp`), so the tile is left out.
#pragma once

#include <string_view>

namespace opensu::library {

/// The Add a game tile's caption; empty where nothing can be added.
[[nodiscard]] std::string_view addGameOffer() noexcept;

} // namespace opensu::library

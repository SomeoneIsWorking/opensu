// match_pick — what the player says a game is, for its artwork: the `match` setting at the game's
// layer. A store game or one the player added is matched to a Steam app, whose covers it then
// shows and SteamGridDB looks it up by; a ROM is matched to a libretro-thumbnails title of its
// system, whose box art it then shows, and an added game may be matched to any system's
// (`boxArtPick`). The pick only changes artwork, never how the game runs.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"
#include "settings/setting_layers.hpp"

namespace opensu::library {

/// The key the pick is kept under at the game layer.
inline constexpr std::string_view matchKey = "match";
/// What starts an added game's pick of box art.
inline constexpr std::string_view boxArtPick = "box:";

/// What a game can be matched to.
enum class MatchKind : std::uint8_t {
    None,
    /// A Steam app id.
    SteamApp,
    /// A title in its system's libretro-thumbnails box art listing.
    LibretroTitle,
};

[[nodiscard]] MatchKind matchKindOf(const Game& game);

/// What `game` can be matched to as its host tells a guest: "steam", "added", "rom:<system>", or
/// empty for nothing.
[[nodiscard]] std::string matchTargetOf(const Game& game);
/// Whether `game`, here or on its host, was added by the player, so it may take any system's art.
[[nodiscard]] bool matchedAsAdded(const Game& game);
/// The key of the system whose box art a `MatchKind::LibretroTitle` game is matched in.
[[nodiscard]] std::string matchSystemOf(const Game& game);

/// Gives each game with a pick that applies its match as `artworkKey`, in place of what its
/// source found, and drops the artwork its source supplied so the match's is shown.
void applyMatchPicks(const settings::SettingLayers& layers, std::vector<Game>& games);

} // namespace opensu::library

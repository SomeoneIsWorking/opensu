// game_matches — the Steam apps a game could be, for Match a different game: Steam's store search
// by name (`identify::storeSearchUrl`, `parseStoreSearch`) and each app's release year from the
// store's `appdetails`.
#pragma once

#include <string>
#include <string_view>

#include "game_identify.hpp"

namespace opensu::library::matches {

/// The store's `appdetails` for `appId`, only its release date.
[[nodiscard]] std::string releaseDateUrl(std::string_view appId,
                                         std::string_view store = identify::steamStore);

/// The year `appId` came out on Steam in an `appdetails` answer; empty when it says none.
[[nodiscard]] std::string parseReleaseYear(std::string_view json, std::string_view appId);

} // namespace opensu::library::matches

// titles — the same game across stores. Store games with the same normalised title are one
// title owned in several stores; this is the one place that merges them.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace opensu::library {

/// `title` as the stores compare it: lower-case letters and digits only, "&" read as "and", so
/// "Hades II", "HADES II™" and "Hades  II" match. Nothing but letters and digits of ASCII
/// count, so a title with none (all CJK, say) has an empty key and is never merged.
[[nodiscard]] std::string titleKey(std::string_view title);

/// One title and every store copy of it.
struct Title {
    /// Preferred first: installed before not, then Steam, GOG, Epic. The first is what an
    /// "All games" entry launches or installs.
    std::vector<const Game*> copies;
};

/// The store games (not ROMs) merged by title. A store holds at most one copy of a title, so two
/// games of one store with the same key stay apart. Ordered by the catalog position of each
/// title's preferred copy. The pointers refer to `games`.
[[nodiscard]] std::vector<Title> storeTitles(const std::vector<Game>& games);

/// Every copy of `game`'s title in `games`, preferred first; `game` alone when it has no other.
[[nodiscard]] std::vector<Game> copiesOf(const std::vector<Game>& games, const Game& game);

} // namespace opensu::library

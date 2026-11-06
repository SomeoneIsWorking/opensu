// titles — the same game across stores. Store games with the same normalised title are one
// title owned in several stores; this is the one place that merges them, and the one rule for
// which copy an entry uses (`TitleCopies`).
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "game.hpp"

namespace opensu::library {

/// `title` as the stores compare it: lower-case letters and digits only, "&" read as "and", so
/// "Hades II", "HADES II™" and "Hades  II" match. Nothing but letters and digits of ASCII
/// count, so a title with none (all CJK, say) has an empty key and is never merged.
[[nodiscard]] std::string titleKey(std::string_view title);

/// What the player's choices about `game` (hidden, on Home) are kept under: its title across
/// stores ("title:<key>"), or its id for a ROM or a title with an empty key ("id:<id>").
[[nodiscard]] std::string choiceKey(const Game& game);

/// `game` as a heading names it: "Hades (Epic)" for a copy of a title two stores own, else its
/// title.
[[nodiscard]] std::string copyName(const Game& game);

/// One title and every store copy of it.
struct Title {
    /// Preferred first: installed before not, then Steam, GOG, Epic.
    std::vector<const Game*> copies;
};

/// The store games (not ROMs) merged by title. A store holds at most one copy of a title, so two
/// games of one store with the same key stay apart. Ordered by the catalog position of each
/// title's preferred copy. The pointers refer to `games`.
[[nodiscard]] std::vector<Title> storeTitles(const std::vector<Game>& games);

/// Every copy of `game`'s title in `games`, preferred first; `game` alone when it has no other.
[[nodiscard]] std::vector<Game> copiesOf(const std::vector<Game>& games, const Game& game);

/// Marks each store copy of a title two stores own (`Game::severalStores`).
void markCopies(std::vector<Game>& games);

/// Which copy of a title an entry uses, over one catalog. There is no saved choice: the order
/// comes from play history.
class TitleCopies {
  public:
    /// `games` outlives this.
    explicit TitleCopies(const std::vector<Game>& games);

    /// Every copy of `game`'s title, top first: installed before not, within that store copies by
    /// when they were last played, latest first, then Steam, GOG, Epic. Empty for a game of no
    /// title.
    [[nodiscard]] std::vector<const Game*> ordered(const Game& game) const;

    /// The first of `ordered`: what an entry shows, and what plays without the details page. Null
    /// for a game of no title, which is only itself.
    [[nodiscard]] const Game* top(const Game& game) const;

    /// The copies the details page gives a button each, top first: the stores' and the top copy.
    /// Empty unless two stores own the title.
    [[nodiscard]] std::vector<Game> offered(const Game& game) const;

    /// Whether any copy of `game`'s title has an update.
    [[nodiscard]] bool updatePending(const Game& game) const;

  private:
    [[nodiscard]] const Title* titleOf(const Game& game) const;

    std::vector<Title> titles_;
    /// A game's id to its place in `titles_`.
    std::unordered_map<std::string, std::size_t> byGame_;
};

} // namespace opensu::library

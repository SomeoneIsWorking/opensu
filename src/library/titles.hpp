// titles — the same game across stores. Store games with the same normalised title are one
// title owned in several stores; this is the one place that merges them, and the one rule for
// which copy an entry uses (`TitleCopies`).
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

/// The store each title uses, as the player chose it, by `choiceKey`. A choice outlives the copy,
/// which a store still loading or signed out brings back.
class CopyChoices {
  public:
    using Entries = std::map<std::string, Source>;

    CopyChoices() = default;
    explicit CopyChoices(Entries entries) : entries_{std::move(entries)} {
    }

    /// The store chosen for `game`'s title; nothing when the player has not chosen.
    [[nodiscard]] std::optional<Source> chosen(const Game& game) const;
    /// Makes `copy` the one its title uses.
    void choose(const Game& copy);

    [[nodiscard]] const Entries& entries() const noexcept {
        return entries_;
    }

    bool operator==(const CopyChoices&) const = default;

  private:
    Entries entries_;
};

/// Which copy of a title an entry uses, over one catalog and the player's choices. `page` is the
/// launcher an entry is shown under or the store filter in force, nothing on Home and All games.
class TitleCopies {
  public:
    /// `games` and `choices` outlive this.
    TitleCopies(const std::vector<Game>& games, const CopyChoices& choices);

    /// The copy `game`'s entry plays and shows: the chosen store's; else `page`'s; else the
    /// preferred one. A page of the player's own games or of another openSU keeps its own copy.
    /// Null for a game of no title, which is only itself.
    [[nodiscard]] const Game* use(const Game& game, std::optional<Source> page) const;

    /// The copies an Install on `game`'s entry offers: the one `use` gives when the player chose
    /// or there is a `page`, else every copy, preferred first.
    [[nodiscard]] std::vector<Game> installable(const Game& game, std::optional<Source> page) const;

    /// The store copies the player can switch `game`'s entry between, in Steam, GOG, Epic order;
    /// empty unless two stores own it, and on a page that keeps its own copy.
    [[nodiscard]] std::vector<Game> switchable(const Game& game, std::optional<Source> page) const;

  private:
    [[nodiscard]] const Title* titleOf(const Game& game) const;

    const CopyChoices& choices_;
    std::vector<Title> titles_;
    /// A game's id to its place in `titles_`.
    std::unordered_map<std::string, std::size_t> byGame_;
};

} // namespace opensu::library

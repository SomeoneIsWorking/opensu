// titles — one game across every source: its versions (ROMs of any console, the player's own
// games, store copies, copies on paired openSUs) grouped by title, the player's groupings and
// separations (`VersionChoices`) applied. This is the one place that groups them, and the one rule
// for which version an entry uses (`TitleCopies`).
#pragma once

#include <cstddef>
#include <functional>
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

/// The `titleKey` of `title` before its subtitle ("X-Men Legends II" of "X-Men Legends II - Rise
/// of Apocalypse" or "…: Rise of Apocalypse"); its whole key when it has none.
[[nodiscard]] std::string baseKey(std::string_view title);

/// One game and every version of it.
struct Title {
    /// Preferred first: Extras last, the player's default first, installed before not, this
    /// machine's before another openSU's, then Steam, GOG, Epic, the player's own, ROMs.
    std::vector<const Game*> copies;
};

/// Every game grouped into its versions. Versions are one game when their `titleKey`s are equal
/// (ROM titles lost their dump tags at the scan), or when one has only the `baseKey` of the other
/// and no other subtitled game shares that base. A store or another openSU holds at most one
/// version of a game, so two of one store with the same key stay apart. A version the player kept
/// apart is a game of its own; one they grouped with another joins that one's game whatever its
/// title. Ordered by the catalog position of each game's preferred version. The pointers refer to
/// `games`.
/// What `game` runs as, as its version's label and another openSU's listing name it: its
/// console, "Windows · Wine", "Linux", "Linux · AppImage", or its store. A remote game's, as its
/// host said.
[[nodiscard]] std::string versionPlatform(const Game& game);

[[nodiscard]] std::vector<Title> gameTitles(const std::vector<Game>& games);

/// Every copy of `game`'s title in `games`, preferred first; `game` alone when it has no other.
[[nodiscard]] std::vector<Game> copiesOf(const std::vector<Game>& games, const Game& game);

/// Marks each store copy of a title two stores own (`Game::severalStores`).
void markCopies(std::vector<Game>& games);

/// A kind of data kept per game, under ids.
struct KeptData {
    std::function<bool(const std::string& id)> holds;
    std::function<void(const std::string& from, const std::string& to)> copy;
};

/// Sets `Game::sharedId`: a title of several stores keeps its data under the title's key, and a
/// title that lost a copy goes on using what it kept there. A kind of data the key has none of yet
/// starts from the most recently played copy that has some, whose own stays where it is. Other
/// games keep their data under their ids. Needs last played applied.
void shareTitleData(std::vector<Game>& games, const std::vector<KeptData>& kept);

/// Which copy of a title an entry uses, over one catalog. There is no saved choice: the order
/// comes from play history.
class TitleCopies {
  public:
    /// `games` outlives this.
    explicit TitleCopies(const std::vector<Game>& games);

    /// Every version of `game`'s game, top first: Extras last, the player's default first,
    /// installed before not, this machine's before another openSU's, within that by when they were
    /// last played, latest first, then Steam, GOG, Epic, the player's own, ROMs. Empty for a game
    /// not in the catalog.
    [[nodiscard]] std::vector<const Game*> ordered(const Game& game) const;

    /// The first of `ordered`: what an entry shows, and what plays without the details page. Null
    /// for a game of no title, which is only itself.
    [[nodiscard]] const Game* top(const Game& game) const;

    /// The versions the details page lists, top first and Extras last; empty for a game of one
    /// version.
    [[nodiscard]] std::vector<Game> offered(const Game& game) const;

    /// Whether any copy of `game`'s title has an update.
    [[nodiscard]] bool updatePending(const Game& game) const;

    /// The other openSUs holding a version of `game`'s game outside Extras, by name, each once,
    /// in version order; `game`'s own host is left out.
    [[nodiscard]] std::vector<std::string> hostsOf(const Game& game) const;

  private:
    [[nodiscard]] const Title* titleOf(const Game& game) const;

    std::vector<Title> titles_;
    /// A game's id to its place in `titles_`.
    std::unordered_map<std::string, std::size_t> byGame_;
};

} // namespace opensu::library

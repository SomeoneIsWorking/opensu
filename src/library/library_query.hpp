// library_query — the one owner of what the player sees of the library: the search text, the
// filters, the sort and the hidden games, applied to the catalog before any shelf is built from
// it. Home, Library, a folder and All games all take their games from here, so a rule is written
// once. The filters and the sort are openSU's own; iiSU has none of them.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"
#include "shelf.hpp"

namespace opensu::library {

/// How games are ordered. `Recent` is the catalog's own order: installed first, then the latest
/// played, then by title.
enum class SortKey : std::uint8_t {
    Recent,
    Name,
    Store,
};

inline constexpr std::array allSortKeys{SortKey::Recent, SortKey::Name, SortKey::Store};

/// The sort as the settings file spells it: "recent", "name", "store".
[[nodiscard]] std::string_view key(SortKey sort) noexcept;
[[nodiscard]] std::optional<SortKey> sortKeyOf(std::string_view key) noexcept;
/// The sort as the player reads it.
[[nodiscard]] std::string_view label(SortKey sort) noexcept;

/// What narrows and orders the library.
struct ViewOptions {
    SortKey sort{SortKey::Recent};
    /// Only games that can be launched now.
    bool installedOnly{false};
    /// Only one store or one system, by its folder key ("launcher:steam", "gc"); empty for all.
    std::string source;
    /// Only the games the player hid.
    bool hiddenOnly{false};
    /// Games whose title holds this text, best match first; empty for no search.
    std::string search;

    bool operator==(const ViewOptions&) const = default;
};

/// The games the player hid. A store game is hidden as a title, so every store's copy goes with
/// it; a ROM is hidden by itself.
class HiddenGames {
  public:
    HiddenGames() = default;
    explicit HiddenGames(std::vector<std::string> keys);

    /// What identifies `game` here: its title across stores, or its id for a ROM.
    [[nodiscard]] static std::string keyOf(const Game& game);

    [[nodiscard]] bool contains(const Game& game) const;
    /// Hides or shows `game`.
    void set(const Game& game, bool hidden);
    /// The keys, sorted, as the settings file keeps them.
    [[nodiscard]] const std::vector<std::string>& keys() const noexcept {
        return keys_;
    }

    bool operator==(const HiddenGames&) const = default;

  private:
    std::vector<std::string> keys_;
};

/// One entry of the source filter.
struct SourceChoice {
    /// The folder key, empty for every source.
    std::string key;
    std::string label;

    bool operator==(const SourceChoice&) const = default;
};

/// "All sources", then each store present and each system with ROMs, in Library's order.
[[nodiscard]] std::vector<SourceChoice> sourceChoices(const std::vector<Game>& games,
                                                      const std::vector<SourceStatus>& sources);

/// Whether anything but the sort narrows the library.
[[nodiscard]] bool narrowing(const ViewOptions& options) noexcept;

/// The catalog as the options and the hidden games leave it, in the sort's order. The text search
/// is not applied here: it ranks, which a filter does not.
struct LibraryView {
    std::vector<Game> games;
    /// The stores the source filter leaves out are `Absent`, so Library shows no launcher for them.
    std::vector<SourceStatus> sources;
};

[[nodiscard]] LibraryView applyOptions(const std::vector<Game>& games,
                                       const std::vector<SourceStatus>& sources,
                                       const ViewOptions& options, const HiddenGames& hidden);

/// What a search for `text` finds in `view`: the folders and the games whose names hold it, those
/// that begin with it first, then those with a word that does, then the rest; each group in the
/// view's order, folders before games.
[[nodiscard]] std::vector<ShelfItem> searchShelf(const LibraryView& view, std::string_view text);

/// What a result is, for its line in the search panel: the stores a game is owned in, a ROM's
/// system, or "Console", "Store" or "Library" for a folder.
[[nodiscard]] std::string describeResult(const ShelfItem& item);

/// The shelf the grid shows: the search's results when there is a search, else the browser's
/// shelf of the narrowed library.
[[nodiscard]] std::vector<ShelfItem> visibleShelf(ShelfBrowser& browser,
                                                  const std::vector<Game>& games,
                                                  const std::vector<SourceStatus>& sources,
                                                  const ViewOptions& options,
                                                  const HiddenGames& hidden);

} // namespace opensu::library

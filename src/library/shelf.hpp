// shelf — what one screen of the grid holds. Home holds the store games and the player's own games
// installed here, less those the player took off it, and the games and consoles the player added
// (`HomeChoices`), or Library's add tiles while it would hold none. Library holds one tile per
// launcher (the player's own games one of them, and one per openSU on the LAN whose games are
// listed), the stores' giveaways, the add tiles, the combined library and one per console with
// ROMs. Opening one of the folders holds its games; Giveaways and the add tiles open screens of
// their own.
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "game.hpp"
#include "home_choices.hpp"
#include "sections.hpp"

namespace opensu::library {

/// A system with ROMs here, standing for all of them in Library.
struct Console {
    /// ES-DE system name, the ROMs' `sourceId` ("gc").
    std::string system;
    /// The name the player reads ("GameCube").
    std::string label;
    std::size_t games{0};

    bool operator==(const Console&) const = default;
};

/// A store present here, standing for every game owned in it, installed or not.
struct Launcher {
    Source source{Source::Steam};
    std::size_t games{0};
    /// Whether the store listed its games on the last read; false when it needs signing in.
    bool ready{true};
    /// Whether its first listing has not come back yet; not `ready`, but nothing to sign in to.
    bool loading{false};
    /// For `Source::Remote`, the openSU whose games it holds: its peer id and name.
    std::string hostId{};
    std::string hostName{};

    bool operator==(const Launcher&) const = default;
};

/// Every store game, a title owned in several stores once.
struct AllGames {
    std::size_t games{0};

    bool operator==(const AllGames&) const = default;
};

/// The stores' free-to-keep offers, beside the launchers; it opens the Giveaways screen.
struct Giveaways {
    bool operator==(const Giveaways&) const = default;
};

/// The Giveaways tile's and screen's name.
inline constexpr std::string_view giveawaysName = "Giveaways";

/// A tile that adds to the library: a game of the player's own, a ROM folder, an integration not
/// set up yet. The shell says which tiles there are and acts on a pressed one by its id.
struct AddTile {
    std::string id;
    std::string name;
    std::string caption;

    bool operator==(const AddTile&) const = default;
};

/// A tile that opens a shelf of its own.
using Folder = std::variant<Console, Launcher, AllGames>;
using ShelfItem = std::variant<Game, Console, Launcher, AllGames, Giveaways, AddTile>;

/// The folder a shelf item opens, or nothing for a game.
[[nodiscard]] std::optional<Folder> folderOf(const ShelfItem& item);

/// The folder as the control channel names it: the system, "launcher:steam",
/// "launcher:lan:<peer id>" or "all".
[[nodiscard]] std::string key(const Folder& folder);

/// The folder's name as the player reads it.
[[nodiscard]] std::string name(const Folder& folder);

/// One console per system with ROMs, in the known systems' order.
[[nodiscard]] std::vector<Console> consoles(const std::vector<Game>& games);

/// Home: the consoles the player added, in Library's order, then the games in `games`' order (the
/// library's sort): each store title whose preferred copy is installed here, a title installed in
/// several stores once, unless the player took it off, and each game the player added, a ROM, a
/// title not installed or one on another openSU. With none of those, the add tiles `adds` once no
/// store in `sources` is still loading its first listing; nothing while one is.
[[nodiscard]] std::vector<ShelfItem> homeShelf(const std::vector<Game>& games,
                                               const std::vector<SourceStatus>& sources,
                                               const std::vector<AddTile>& adds,
                                               const HomeChoices& choices);

/// Whether `item` is on Home: a game as its title's preferred copy in `games`, or a console;
/// nothing for a tile that cannot be put on Home.
[[nodiscard]] std::optional<bool> onHome(const std::vector<Game>& games, const ShelfItem& item,
                                         const HomeChoices& choices);

/// Puts `item`, a game or a console, on Home or takes it off.
void setOnHome(const std::vector<Game>& games, const ShelfItem& item, bool on,
               HomeChoices& choices);

/// Library: one launcher per store that is not absent, Steam, Epic, GOG, then the player's own
/// games when there are any, then one per openSU on the LAN with games listed, by its name;
/// Giveaways when a store has a launcher; the add tiles `adds`, in their order; the combined
/// library when a store or the player has games; then one console per system with ROMs, in the
/// known systems' order.
[[nodiscard]] std::vector<ShelfItem> libraryShelf(const std::vector<Game>& games,
                                                  const std::vector<SourceStatus>& sources,
                                                  const std::vector<AddTile>& adds);

/// One console's ROMs, in catalog order.
[[nodiscard]] std::vector<ShelfItem> consoleShelf(const std::vector<Game>& games,
                                                  std::string_view system);

/// One launcher's whole library, installed or not, in catalog order: a store's, or one openSU's on
/// the LAN. A tile here stands for that launcher's game, so `ownedIn` stays empty.
[[nodiscard]] std::vector<ShelfItem> launcherShelf(const std::vector<Game>& games,
                                                   const Launcher& launcher);

/// Every store game, each title once as its preferred copy, with every store it is owned in.
[[nodiscard]] std::vector<ShelfItem> allGamesShelf(const std::vector<Game>& games);

/// The games a folder holds.
[[nodiscard]] std::vector<ShelfItem> folderShelf(const std::vector<Game>& games,
                                                 const Folder& folder);

/// Which shelf the grid shows: a section's, or one folder's inside it.
class ShelfBrowser {
  public:
    /// The shelf for the current place. A folder that has emptied returns to its section, to the
    /// slot it was entered from (`takeReturn`).
    [[nodiscard]] std::vector<ShelfItem> shelf(const std::vector<Game>& games,
                                               const std::vector<SourceStatus>& sources,
                                               const std::vector<AddTile>& adds,
                                               const HomeChoices& home);

    /// The slot to focus in the section an emptied folder returned to, once; nothing when none
    /// did since the last call.
    [[nodiscard]] std::optional<std::size_t> takeReturn() noexcept {
        return std::exchange(returned_, std::nullopt);
    }

    /// Opens a folder, remembering which slot of the section's shelf had focus.
    void open(const Folder& folder, std::size_t sectionFocus);

    /// Whether a folder is open, which is when `back` does anything.
    [[nodiscard]] bool canBack() const noexcept {
        return folder_.has_value();
    }

    /// Returns to the section's shelf and gives the slot to focus there, or nothing when no folder
    /// is open.
    [[nodiscard]] std::optional<std::size_t> back();

    /// Remembers `focus`, the grid's focus now, as where the active section is left.
    void leave(std::size_t focus);

    /// Moves `delta` sections along the dock, closing any folder; the result is the slot to focus
    /// in the section arrived at, where it was last left.
    [[nodiscard]] std::size_t cycle(int delta);

    [[nodiscard]] Section section() const noexcept {
        return sections_.active();
    }

    /// The open folder, or nothing at a section's top.
    [[nodiscard]] const std::optional<Folder>& folder() const noexcept {
        return folder_;
    }

    /// The launcher whose library is open, or nothing.
    [[nodiscard]] std::optional<Source> launcher() const noexcept {
        const Launcher* open = folder_ ? std::get_if<Launcher>(&*folder_) : nullptr;
        return open == nullptr ? std::nullopt : std::optional{open->source};
    }

  private:
    Sections sections_;
    std::optional<Folder> folder_;
    /// The focus the open folder was entered from, in its section's shelf.
    std::size_t parentFocus_{0};
    /// Where an emptied folder returned to, until taken.
    std::optional<std::size_t> returned_;
    /// Where each section was left, by `Section`.
    std::array<std::size_t, allSections.size()> left_{};
};

} // namespace opensu::library

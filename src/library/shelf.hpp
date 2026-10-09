// shelf — what one screen of the grid holds. Home holds the store games installed here. Library
// holds one tile per launcher, the combined library and one per console with ROMs. Opening one of
// those holds its games.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "game.hpp"
#include "sections.hpp"

namespace opensu::library {

/// A system with ROMs here, standing for all of them in Library.
struct Console {
    /// ES-DE system name, the ROMs' `sourceId` ("gc").
    std::string system;
    /// The name the player reads ("GameCube").
    std::string label;
    std::size_t games{0};
    /// The console card on disk, or empty for none.
    std::filesystem::path artwork;

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

    bool operator==(const Launcher&) const = default;
};

/// Every store game, a title owned in several stores once.
struct AllGames {
    std::size_t games{0};

    bool operator==(const AllGames&) const = default;
};

/// A tile that opens a shelf of its own.
using Folder = std::variant<Console, Launcher, AllGames>;
using ShelfItem = std::variant<Game, Console, Launcher, AllGames>;

/// The folder a shelf item opens, or nothing for a game.
[[nodiscard]] std::optional<Folder> folderOf(const ShelfItem& item);

/// The folder as the control channel names it: the system, "launcher:steam" or "all".
[[nodiscard]] std::string key(const Folder& folder);

/// The folder's name as the player reads it.
[[nodiscard]] std::string name(const Folder& folder);

/// One console per system with ROMs, in the known systems' order.
[[nodiscard]] std::vector<Console> consoles(const std::vector<Game>& games);

/// Home: the store games installed here, a title installed in several stores once.
[[nodiscard]] std::vector<ShelfItem> homeShelf(const std::vector<Game>& games);

/// Library: one launcher per store that is not absent, Steam, Epic, GOG; the combined library when
/// a store has games; then one console per system with ROMs, in the known systems' order.
[[nodiscard]] std::vector<ShelfItem> libraryShelf(const std::vector<Game>& games,
                                                  const std::vector<SourceStatus>& sources);

/// One console's ROMs, in catalog order.
[[nodiscard]] std::vector<ShelfItem> consoleShelf(const std::vector<Game>& games,
                                                  std::string_view system);

/// One store's whole library, installed or not, in catalog order. A game owned in other stores
/// lists them in `ownedIn`.
[[nodiscard]] std::vector<ShelfItem> launcherShelf(const std::vector<Game>& games, Source source);

/// Every store game, each title once as its preferred copy, with every store it is owned in.
[[nodiscard]] std::vector<ShelfItem> allGamesShelf(const std::vector<Game>& games);

/// The games a folder holds.
[[nodiscard]] std::vector<ShelfItem> folderShelf(const std::vector<Game>& games,
                                                 const Folder& folder);

/// Which shelf the grid shows: a section's, or one folder's inside Library.
class ShelfBrowser {
  public:
    /// The shelf for the current place. A folder that has emptied returns to its section.
    [[nodiscard]] std::vector<ShelfItem> shelf(const std::vector<Game>& games,
                                               const std::vector<SourceStatus>& sources);

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

    /// Whether the open folder is a launcher's own library.
    [[nodiscard]] bool inLauncher() const noexcept {
        return folder_ && std::holds_alternative<Launcher>(*folder_);
    }

  private:
    Sections sections_;
    std::optional<Folder> folder_;
    /// The focus the open folder was entered from, in its section's shelf.
    std::size_t parentFocus_{0};
    /// Where each section was left, by `Section`.
    std::array<std::size_t, allSections.size()> left_{};
};

} // namespace opensu::library

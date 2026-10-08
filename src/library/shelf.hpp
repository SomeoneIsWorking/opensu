// shelf — what one screen of the home grid holds. Home holds one tile per launcher, the combined
// library, one per console with ROMs and the store games installed here. Opening a console, a
// launcher or the combined library holds its games.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "game.hpp"

namespace iideck::library {

/// A system with ROMs here, standing for all of them on Home.
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

/// Home: one launcher per store that is not absent, Steam, Epic, GOG; the combined library when
/// a store has games; one console per system with ROMs, in the known systems' order; then the
/// store games installed here, a title installed in several stores once.
[[nodiscard]] std::vector<ShelfItem> homeShelf(const std::vector<Game>& games,
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

/// Which shelf the grid shows: Home, or one folder.
class ShelfBrowser {
  public:
    /// The shelf for the current place. A folder that has emptied returns to Home.
    [[nodiscard]] std::vector<ShelfItem> shelf(const std::vector<Game>& games,
                                               const std::vector<SourceStatus>& sources);

    /// Opens a folder, remembering which Home slot had focus.
    void open(const Folder& folder, std::size_t homeFocus);

    /// Returns to Home and gives the slot to focus there, or nothing when already Home.
    [[nodiscard]] std::optional<std::size_t> back();

    /// The open folder, or nothing on Home.
    [[nodiscard]] const std::optional<Folder>& folder() const noexcept {
        return folder_;
    }

    /// Whether the open folder is a launcher's own library.
    [[nodiscard]] bool inLauncher() const noexcept {
        return folder_ && std::holds_alternative<Launcher>(*folder_);
    }

  private:
    std::optional<Folder> folder_;
    std::size_t homeFocus_{0};
};

} // namespace iideck::library

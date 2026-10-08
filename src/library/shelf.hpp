// shelf — what one screen of the home grid holds. Home holds the stores' games and one console
// per system that has ROMs; opening a console holds that system's ROMs.
#pragma once

#include <cstddef>
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

    bool operator==(const Console&) const = default;
};

using ShelfItem = std::variant<Game, Console>;

/// Home: one console per system with ROMs, in the known systems' order, then every other game
/// in catalog order.
[[nodiscard]] std::vector<ShelfItem> homeShelf(const std::vector<Game>& games);

/// One console's ROMs, in catalog order.
[[nodiscard]] std::vector<ShelfItem> consoleShelf(const std::vector<Game>& games,
                                                  std::string_view system);

/// Which shelf the grid shows: Home, or one console's ROMs.
class ShelfBrowser {
  public:
    /// The shelf for the current place. A console whose ROMs are gone returns to Home.
    [[nodiscard]] std::vector<ShelfItem> shelf(const std::vector<Game>& games);

    /// Opens a console, remembering which Home slot had focus.
    void open(const Console& console, std::size_t homeFocus);

    /// Returns to Home and gives the slot to focus there, or nothing when already Home.
    [[nodiscard]] std::optional<std::size_t> back();

    /// The open console, or nothing on Home.
    [[nodiscard]] const std::optional<Console>& console() const noexcept {
        return console_;
    }

  private:
    std::optional<Console> console_;
    std::size_t homeFocus_{0};
};

} // namespace iideck::library

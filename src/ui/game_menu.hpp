// game_menu — the menu Guide opens over a running game: resume it or close it.
//
// opensu's own, after Steam's in-game Guide menu; iiSU has no counterpart. Pure state, so it is
// tested without a window.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "home_layout.hpp"

namespace opensu::ui {

enum class GameMenuAction : std::uint8_t { Resume, CloseGame };

struct GameMenuItem {
    const char* label;
    GameMenuAction action;
};

/// How many items the menu holds.
inline constexpr std::size_t gameMenuItemCount = 2;

/// The panel down the left edge and its item rows, in pixels.
struct GameMenuLayout {
    Rect panel;
    float padding{};
    std::array<Rect, gameMenuItemCount> rows;

    /// The row under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> itemAt(float x, float y) const noexcept;
};

/// The layout in a `width` x `height` frame at `dp` pixels per dp; `titleBox` is the title line's
/// height, which the rows start below.
[[nodiscard]] GameMenuLayout layoutGameMenu(float width, float height, float dp,
                                            float titleBox) noexcept;

class GameMenu {
  public:
    /// Opens the menu over `title`, focused on its first item.
    void open(std::string title);
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }

    [[nodiscard]] std::span<const GameMenuItem> items() const noexcept {
        return items_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Moves focus by `delta` items, stopping at either end.
    void move(int delta) noexcept;
    /// Focuses item `index`; reports whether focus changed.
    bool focusItem(std::size_t index) noexcept;
    /// The focused item's action.
    [[nodiscard]] GameMenuAction selected() const noexcept {
        return items_[focus_].action;
    }

  private:
    static constexpr std::array<GameMenuItem, gameMenuItemCount> items_{
        GameMenuItem{"Resume", GameMenuAction::Resume},
        GameMenuItem{"Close game", GameMenuAction::CloseGame},
    };
    std::string title_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace opensu::ui

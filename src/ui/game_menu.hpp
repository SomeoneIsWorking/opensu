// game_menu — the menu Guide opens over a running game: resume it or close it.
//
// iideck's own, after Steam's in-game Guide menu; iiSU has no counterpart. Pure state, so it is
// tested without a window.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace iideck::ui {

enum class GameMenuAction : std::uint8_t { Resume, CloseGame };

struct GameMenuItem {
    const char* label;
    GameMenuAction action;
};

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
    /// The focused item's action.
    [[nodiscard]] GameMenuAction selected() const noexcept {
        return items_[focus_].action;
    }

  private:
    static constexpr std::array<GameMenuItem, 2> items_{
        GameMenuItem{"Resume", GameMenuAction::Resume},
        GameMenuItem{"Close game", GameMenuAction::CloseGame},
    };
    std::string title_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace iideck::ui

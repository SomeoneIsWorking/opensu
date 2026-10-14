// context_menu — the menu a game or a folder tile opens (iiSU's ROM context menu, `o73`: SELECT
// on a focused item, screens.md §4): a glass card in the middle of the frame, titled with the tile
// and listing what can be done with it. Pure state and geometry, tested without a window.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "library/shelf.hpp"

namespace opensu::ui {

enum class ContextAction : std::uint8_t {
    /// Launch an installed game.
    Launch,
    /// Offer to install a game that is not.
    Install,
    /// Show a game's details.
    Details,
    Hide,
    Unhide,
    /// Open a folder.
    Open,
    /// Read the stores again.
    Refresh,
    /// Sign in to the store a launcher stands for.
    SignIn,
    /// Uninstall an installed store game, on a second press.
    Uninstall,
};

struct ContextItem {
    std::string label;
    ContextAction action;

    bool operator==(const ContextItem&) const = default;
};

/// What the menu offers for `item`, first entry first. `hidden` is whether a game is hidden now.
[[nodiscard]] std::vector<ContextItem> contextItemsFor(const library::ShelfItem& item, bool hidden);

/// The card and its rows in pixels.
struct ContextLayout {
    Rect card;
    Rect title;
    std::vector<Rect> rows;
    /// The line the button hints stand on.
    Rect hints;
    float radius{};

    /// The row under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> itemAt(float x, float y) const noexcept;
    /// Whether the point is on the card.
    [[nodiscard]] bool contains(float x, float y) const noexcept {
        return card.contains(x, y);
    }
};

[[nodiscard]] ContextLayout layoutContextMenu(const Rect& frame, float dp, std::size_t items);

class ContextMenu {
  public:
    /// Opens over `title` with `items`, focused on the first.
    void open(std::string title, std::vector<ContextItem> items);
    void close() noexcept {
        open_ = false;
        confirm_.clear();
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::vector<ContextItem>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Moves focus by `delta` items, stopping at the ends; reports whether it moved. Moving lets
    /// go of an armed item.
    bool move(int delta) noexcept;
    /// Focuses item `index`; reports whether focus changed, which lets go of an armed item.
    bool focusItem(std::size_t index) noexcept;

    /// Asks the focused item for a second press; `confirm` says what that press does.
    void arm(std::string confirm) {
        confirm_ = std::move(confirm);
    }
    [[nodiscard]] bool armed() const noexcept {
        return !confirm_.empty();
    }
    /// Item `index`'s label as the menu reads it: an armed one says what the second press does.
    [[nodiscard]] std::string shown(std::size_t index) const;
    [[nodiscard]] ContextAction selected() const noexcept {
        return items_[focus_].action;
    }
    [[nodiscard]] ContextLayout layout(const Rect& frame, float dp) const {
        return layoutContextMenu(frame, dp, items_.size());
    }

  private:
    std::string title_;
    std::vector<ContextItem> items_;
    std::size_t focus_{0};
    /// What the armed item's second press does; empty when none is armed.
    std::string confirm_;
    bool open_{false};
};

} // namespace opensu::ui

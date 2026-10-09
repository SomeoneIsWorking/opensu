// details_page — the page a game opens on (A on its tile, Y, or Details in its menu): the cover and
// title, what is known about the game, and the buttons that act on it. opensu's own; iiSU shows a
// game's details only on the second screen of a dual-display device. Pure state and geometry,
// tested without a window; `DetailsPagePainter` draws it.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "library/game.hpp"

namespace opensu::ui {

enum class DetailsAction : std::uint8_t {
    /// Play an installed game, or offer to install one that is not.
    Launch,
    /// Choose the next emulator for a ROM.
    Emulator,
    /// Hide the game, or show it again.
    Hidden,
    Back,
};

/// A button on the page. `value` is the choice a button shows beside its label, empty for none.
struct DetailsButton {
    DetailsAction action{DetailsAction::Back};
    std::string label;
    std::string value;

    bool operator==(const DetailsButton&) const = default;
};

/// One line of what is known about the game.
struct DetailsRow {
    std::string label;
    std::string value;

    bool operator==(const DetailsRow&) const = default;
};

/// Everything the page shows for one game.
struct DetailsView {
    std::string gameId;
    std::string title;
    /// The store or system the game is from, as the badge reads it.
    std::string badge;
    std::vector<DetailsRow> rows;
    std::vector<DetailsButton> buttons;

    bool operator==(const DetailsView&) const = default;
};

/// When the game was last played, in words: "Never played", "3 days ago", or the date past a month.
[[nodiscard]] std::string
lastPlayedText(const std::optional<std::chrono::system_clock::time_point>& at,
               std::chrono::system_clock::time_point now);

/// What the page shows for `game`; `hidden` is whether the player hid it.
[[nodiscard]] DetailsView detailsFor(const library::Game& game, bool hidden,
                                     std::chrono::system_clock::time_point now);

/// The page and its buttons in pixels.
struct DetailsLayout {
    /// The cover's box.
    Rect art;
    /// Where the title, badge and rows flow, right of the cover and above the buttons.
    Rect text;
    std::vector<Rect> buttons;
    float radius{};

    /// The button under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> buttonAt(float x, float y) const noexcept;
};

/// The page between `topInset` and `bottomInset` in a `width` x `height` frame at `dp` pixels per
/// dp, with `buttons` buttons.
[[nodiscard]] DetailsLayout layoutDetails(float width, float height, float dp, float topInset,
                                          float bottomInset, std::size_t buttons);

class DetailsPage {
  public:
    /// Opens on `view`, focused on the first button.
    void open(DetailsView view);
    /// Shows `view` in place of the one open, keeping focus on the same action when it is still
    /// there. The page closes itself when `view` is for another game.
    void refresh(DetailsView view);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const DetailsView& view() const noexcept {
        return view_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Moves focus by `delta` buttons, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses button `index`; reports whether focus changed.
    bool focusButton(std::size_t index) noexcept;
    [[nodiscard]] DetailsAction selected() const noexcept {
        return view_.buttons[focus_].action;
    }

  private:
    DetailsView view_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace opensu::ui

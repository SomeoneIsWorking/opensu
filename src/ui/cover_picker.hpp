// cover_picker — "Choose artwork": a card over the details page listing the covers a game can
// show, the one it shows now first, then its own when another replaces it, then SteamGridDB's,
// one row of cells that scrolls with the focus. Pure state and geometry; the textures are
// `CoverPickerArt`'s.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

/// One cover the picker offers.
struct CoverPick {
    /// Under the cell: "Now", "Own" or the picture's size.
    std::string label;
    /// The image, or empty while it downloads.
    std::filesystem::path file;

    bool operator==(const CoverPick&) const = default;
};

/// The card and its cells in pixels.
struct CoverPickerLayout {
    Rect card;
    Rect title;
    Rect status;
    /// The cells shown, for the picks from `first` on.
    std::vector<Rect> cells;
    std::size_t first{0};
    /// How many cells fit in the row.
    std::size_t capacity{1};
    /// The line the button hints stand on.
    Rect hints;
    float radius{};

    /// The pick under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> pickAt(float x, float y) const noexcept;
};

/// The layout for `count` picks with the row scrolled to `first`.
[[nodiscard]] CoverPickerLayout layoutCoverPicker(const Rect& frame, float dp, std::size_t count,
                                                  std::size_t first);

class CoverPicker {
  public:
    /// Opens titled `title` with `picks`, focused on the first.
    void open(std::string title, std::vector<CoverPick> picks, std::string status);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    /// Replaces the picks and the status line, keeping the focus where it can.
    void update(std::vector<CoverPick> picks, std::string status);

    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::string& status() const noexcept {
        return status_;
    }
    [[nodiscard]] const std::vector<CoverPick>& picks() const noexcept {
        return picks_;
    }
    /// Changes on each open, so textures of the picks before are dropped.
    [[nodiscard]] std::uint64_t generation() const noexcept {
        return generation_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Moves focus by `delta` picks, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses pick `index`; reports whether focus changed.
    bool focusPick(std::size_t index) noexcept;
    /// How many cells the frame fits, which the row scrolls by; the shell sets it from the frame.
    void setCapacity(std::size_t capacity) noexcept;

    [[nodiscard]] CoverPickerLayout layout(const Rect& frame, float dp) const {
        return layoutCoverPicker(frame, dp, picks_.size(), first_);
    }

  private:
    void keepFocusShown() noexcept;

    std::string title_;
    std::string status_;
    std::vector<CoverPick> picks_;
    std::uint64_t generation_{0};
    std::size_t focus_{0};
    std::size_t first_{0};
    std::size_t capacity_{1};
    bool open_{false};
};

} // namespace opensu::ui

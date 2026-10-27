// downloads_page — the Downloads page: one row per download, each with its title, where it comes
// from, its state and figures, a progress bar and the buttons its owner allows. The pad moves
// between rows with Up and Down and between a row's buttons with Left and Right. iiSU's Active
// Tasks list (`ye4`, one row per task with its title, progress and a cancel) is the model; the
// page is openSU's own. Pure state and geometry, tested without a window; `DownloadsPagePainter`
// draws it and `app::DownloadsController` fills and acts on it.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

/// One download as the page shows it.
struct DownloadsEntry {
    /// The controller's name for the row, stable across refreshes.
    std::string key{};
    std::string title{};
    /// Where it comes from: "Steam", "Steam tool", "Epic", "GOG", "Emulator", "Core".
    std::string source{};
    /// "Downloading", "Paused", "Failed" and so on.
    std::string state{};
    /// The figures after the state: percent, bytes, rate, time left; or why it failed.
    std::string figures{};
    /// 0 to 1 when measured; the bar is drawn only then.
    std::optional<double> fraction{};
    bool failed{false};
    /// Over: drawn quieter, with no buttons.
    bool ended{false};
    std::vector<std::string> actions{};

    bool operator==(const DownloadsEntry&) const = default;
};

/// Where the page's parts stand in a frame, in pixels.
struct DownloadsLayout {
    /// The card the rows scroll in.
    Rect panel;
    float radius{};
    std::vector<Rect> rows{};
    /// The room for the text of each row, left of its buttons.
    std::vector<Rect> bodies{};
    /// The progress bar of each row, under its text.
    std::vector<Rect> bars{};
    /// Each row's buttons, left to right.
    std::vector<std::vector<Rect>> actions{};

    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
    /// The row and button under the point, or nothing.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    actionAt(float x, float y) const noexcept;
};

class DownloadsPage {
  public:
    /// Opens on the first row and its first button.
    void open(std::vector<DownloadsEntry> entries);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// Shows `entries` in place of the ones shown, keeping the focused row (by key) and button
    /// where they still are.
    void refresh(std::vector<DownloadsEntry> entries);

    [[nodiscard]] const std::vector<DownloadsEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::size_t row() const noexcept {
        return row_;
    }
    [[nodiscard]] std::size_t action() const noexcept {
        return action_;
    }
    /// The focused row, or null when there is none.
    [[nodiscard]] const DownloadsEntry* focused() const;

    /// Moves focus `delta` rows, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Moves focus `delta` buttons along the focused row, stopping at the ends.
    bool step(int delta) noexcept;
    /// Focuses row `index`, on its first button; reports whether anything changed.
    bool focusRow(std::size_t index) noexcept;
    /// Focuses button `action` of row `index`; reports whether anything changed.
    bool focusAction(std::size_t index, std::size_t action) noexcept;

    /// Where the page stands in `room`, the frame between its top and bottom bars, at `dp` pixels
    /// per dp, with the focused row scrolled whole into view.
    [[nodiscard]] DownloadsLayout layout(const Rect& room, float dp) const;

  private:
    std::vector<DownloadsEntry> entries_;
    std::size_t row_{0};
    std::size_t action_{0};
    bool open_{false};
};

} // namespace opensu::ui

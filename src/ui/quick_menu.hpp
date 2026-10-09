// quick_menu — the quick menu Guide + A opens: a panel down the right edge for the changes made
// in place (volume, output, Bluetooth, controller batteries, close game), after Steam Deck's quick
// access menu. Its rows are the Settings screen's rows, so a slider or a switch looks and acts the
// same in both. Pure state and geometry, tested without a window.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "panel_frame.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

/// The panel down the right edge and its rows, in pixels.
struct QuickLayout {
    Rect panel;
    float padding{};
    /// Where the heading is drawn.
    Rect heading;
    /// The part of the panel the rows scroll in.
    Rect content;
    std::vector<SettingsRowBox> rows;

    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
    /// The level a point on row `index`'s slider track stands for, or nothing off it.
    [[nodiscard]] std::optional<int> levelAt(const SettingsRow& row, std::size_t index, float x,
                                             float y) const noexcept;
};

/// The layout of `rows` in `frame`, with the row `focused` scrolled whole into view;
/// `chrome.heading` is the heading line's height and `chrome.footer` the room the hints take at the
/// bottom.
[[nodiscard]] QuickLayout layoutQuick(const PanelFrame& frame, const PanelChrome& chrome,
                                      const std::vector<SettingsRow>& rows, std::size_t focused);

class QuickMenu {
  public:
    /// Opens on the first row.
    void open(std::vector<SettingsRow> rows);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// Shows `rows` in place of the ones open, keeping the focus on the same row (by id).
    void refresh(std::vector<SettingsRow> rows);

    [[nodiscard]] const std::vector<SettingsRow>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    [[nodiscard]] const SettingsRow* focusedRow() const;
    /// Moves focus `delta` rows, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Focuses row `index`; reports whether focus changed.
    bool focusRow(std::size_t index) noexcept;

  private:
    std::vector<SettingsRow> rows_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace opensu::ui

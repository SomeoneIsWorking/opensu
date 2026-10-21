// settings_tabs — a panel down the right edge with a row of tabs (L1 and R1 move between them) over
// rows that may carry an Override checkbox: the game settings page of the quick menu and of the
// details page. A row with the checkbox has two places to focus, the checkbox and its value; its
// value takes focus only while it is overridden. Pure state and geometry, tested without a window;
// `SettingsTabsPainter` draws it and `app::GameSettingsPage` fills and acts on it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "panel_frame.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

/// One row: a Settings row, and its Override checkbox when it has one.
struct TabRow {
    SettingsRow row;
    /// The row has an Override checkbox.
    bool overridable{false};
    /// The checkbox is ticked: the value is this layer's own and can be opened.
    bool overridden{false};

    bool operator==(const TabRow&) const = default;
};

/// Where a row's focus is.
enum class TabColumn : std::uint8_t { Override, Value };

/// What the page shows: its heading, its tabs with the one on show, and that tab's rows.
struct TabsView {
    std::string title;
    std::vector<std::string> tabs;
    std::size_t tab{0};
    std::vector<TabRow> rows;

    bool operator==(const TabsView&) const = default;
};

/// The page's parts in pixels.
struct TabsLayout {
    Rect panel;
    float padding{};
    Rect heading;
    /// The strip the tabs stand in, the shoulder glyphs at its ends.
    Rect strip;
    std::vector<Rect> tabs;
    /// The part of the panel the rows scroll in.
    Rect content;
    std::vector<Rect> rows;
    /// Each row's checkbox; empty for a row without one.
    std::vector<Rect> checkboxes;
    /// Each row's value at its end.
    std::vector<Rect> values;

    [[nodiscard]] std::optional<std::size_t> tabAt(float x, float y) const noexcept;
    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
    /// The part of row `index` under the point: its checkbox, else its value.
    [[nodiscard]] TabColumn columnAt(std::size_t index, float x, float y) const noexcept;
};

/// The layout of `view` in `frame`, row `focused` scrolled whole into view; `chrome.heading` is
/// the heading line's height and `chrome.footer` the hints' room. `labels` are the tabs' label
/// widths in pixels: each tab gets its label's room, and what the strip has over is shared out;
/// when the strip is short every tab gives up the same share.
[[nodiscard]] TabsLayout layoutTabs(const PanelFrame& frame, const PanelChrome& chrome,
                                    const TabsView& view, std::size_t focused,
                                    const std::vector<float>& labels);

class SettingsTabs {
  public:
    /// Opens on `view`, focused on its first row's checkbox, or value when it has none.
    void open(TabsView view);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    /// Shows `view` in place of the one open: on the same tab the focus stays on the same row (by
    /// id), on another it goes to the first row.
    void refresh(TabsView view);

    [[nodiscard]] const TabsView& view() const noexcept {
        return view_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Where the focused row's focus is: the checkbox of a row that is not overridden, the value
    /// of a row without a checkbox.
    [[nodiscard]] TabColumn column() const noexcept;
    [[nodiscard]] const TabRow* focusedRow() const;

    /// Moves focus `delta` rows, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    /// Moves focus within the row to `column`; reports whether it moved.
    bool moveColumn(TabColumn column) noexcept;
    /// Focuses `column` of row `index`; reports whether focus changed.
    bool focusRow(std::size_t index, TabColumn column) noexcept;

  private:
    TabsView view_;
    std::size_t focus_{0};
    /// The column asked for, kept while rows without a value to focus pass under it.
    TabColumn column_{TabColumn::Override};
    bool open_{false};
};

} // namespace opensu::ui

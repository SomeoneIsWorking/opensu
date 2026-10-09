// settings_page — the Settings screen: a list of categories on the left and the focused category's
// rows on the right, as iiSU's settings dialog lays its sections and pages out (screens.md §3.4).
// Rows are toggles, choices, a slider, folders, plain actions and read-only facts. Pure state and
// geometry, tested without a window; `SettingsPagePainter` draws it and `app::SettingsController`
// fills and acts on it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

enum class RowKind : std::uint8_t {
    /// On or off.
    Toggle,
    /// One of a few values, stepped with Left and Right.
    Choice,
    /// A level between `low` and `high`, stepped with Left and Right.
    Slider,
    /// A folder, chosen on the folder chooser.
    Folder,
    /// Does something when pressed.
    Action,
    /// A fact; nothing to press.
    Info,
};

/// One line of a category.
struct SettingsRow {
    /// The controller's name for the row, stable across refreshes.
    std::string id;
    RowKind kind{RowKind::Info};
    std::string label;
    /// A short line under the label saying what the row does or where its value comes from.
    std::string note;
    /// What a choice, folder or action reads: the chosen value, the folder, or the result.
    std::string value;
    bool on{false};
    int level{0};
    int low{0};
    int high{0};

    bool operator==(const SettingsRow&) const = default;
};

struct SettingsCategory {
    std::string id;
    std::string label;
    std::vector<SettingsRow> rows;

    bool operator==(const SettingsCategory&) const = default;
};

/// Where the pad is: in the list of categories or in the rows of the focused one.
enum class SettingsZone : std::uint8_t { Categories, Rows };

struct SettingsRowBox {
    Rect rect;
    /// The switch, the slider's track or the value's room; empty for a row with none.
    Rect control;
};

/// Where the page's parts stand in a frame, in pixels.
struct SettingsLayout {
    Rect categories;
    std::vector<Rect> categoryCells;
    /// The card the rows scroll in.
    Rect panel;
    /// The rows of the focused category, scrolled; a row may stand partly outside `panel`.
    std::vector<SettingsRowBox> rows;
    float radius{};
    float cellRadius{};

    [[nodiscard]] std::optional<std::size_t> categoryAt(float x, float y) const noexcept;
    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
    /// The level a point on row `index`'s slider track stands for, or nothing off it.
    [[nodiscard]] std::optional<int> levelAt(const SettingsRow& row, std::size_t index, float x,
                                             float y) const noexcept;
};

/// The page between `topInset` and `bottomInset` in a `width` x `height` frame at `dp` pixels per
/// dp, showing `rows` of the category, with row `focused` scrolled whole into view.
[[nodiscard]] SettingsLayout layoutSettings(float width, float height, float dp, float topInset,
                                            float bottomInset, std::size_t categories,
                                            const std::vector<SettingsRow>& rows,
                                            std::size_t focused);

class SettingsPage {
  public:
    /// Opens on the first category with the pad in the list of categories.
    void open(std::vector<SettingsCategory> categories);
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// Shows `categories` in place of the ones open, keeping the focused category, row and zone
    /// (by id) where they still are.
    void refresh(std::vector<SettingsCategory> categories);

    [[nodiscard]] const std::vector<SettingsCategory>& categories() const noexcept {
        return categories_;
    }
    [[nodiscard]] std::size_t category() const noexcept {
        return category_;
    }
    [[nodiscard]] const SettingsCategory& focusedCategory() const {
        return categories_[category_];
    }
    [[nodiscard]] std::size_t row() const noexcept {
        return row_;
    }
    [[nodiscard]] SettingsZone zone() const noexcept {
        return zone_;
    }
    /// The focused row, or null in the list of categories or when the category has none.
    [[nodiscard]] const SettingsRow* focusedRow() const;

    /// Moves focus `delta` places along the zone's list, stopping at the ends; reports whether
    /// it moved.
    bool move(int delta) noexcept;
    /// Puts the pad in the rows of the focused category; false when it has none.
    bool enterRows() noexcept;
    /// Puts the pad back in the list of categories; false when it already was.
    bool leaveRows() noexcept;
    /// Focuses category `index`, leaving the rows; reports whether anything changed.
    bool focusCategory(std::size_t index) noexcept;
    /// Focuses row `index` of the focused category and enters the rows; reports whether anything
    /// changed.
    bool focusRow(std::size_t index) noexcept;

    /// Where the page stands in a frame.
    [[nodiscard]] SettingsLayout layout(float width, float height, float dp, float topInset,
                                        float bottomInset) const;

  private:
    std::vector<SettingsCategory> categories_;
    std::size_t category_{0};
    std::size_t row_{0};
    SettingsZone zone_{SettingsZone::Categories};
    bool open_{false};
};

} // namespace opensu::ui

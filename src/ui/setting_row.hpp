// setting_row — a settings-like row as its owner describes it: what kind it is, its words and its
// value. The Settings screen turns each into the toolkit's widget of that kind; the Devices page,
// the quick menu and the game settings tabs still draw them their own way.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opensu::ui {

enum class RowKind : std::uint8_t {
    /// On or off.
    Toggle,
    /// One of `choices`, stepped with Left and Right.
    Choice,
    /// A level between `low` and `high`, stepped with Left and Right.
    Slider,
    /// A folder, chosen on the folder chooser.
    Folder,
    /// Does something when pressed.
    Action,
    /// A fact; nothing to press.
    Info,
    /// A heading over the rows after it; nothing to press.
    Heading,
    /// Opens a page of more rows, named by the row's id.
    Page,
};

/// Whether A opens the row of `kind` so Left and Right can change it: a choice or a slider.
[[nodiscard]] bool rowOpens(RowKind kind) noexcept;

/// Whether a row is open for Left and Right: the state A sets and A or B clears. The Devices page
/// and the quick menu each keep one, so a row behaves the same wherever it is.
class RowEditing {
  public:
    [[nodiscard]] bool open() const noexcept {
        return open_;
    }
    /// Opens a row of `kind`; false when it is not one Left and Right change.
    bool begin(RowKind kind) noexcept;
    /// Closes the open row; false when none was open.
    bool end() noexcept;

  private:
    bool open_{false};
};

/// One line of a category.
struct SettingsRow {
    /// The owner's name for the row, stable across refreshes.
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
    /// How far Left or Right moves a slider.
    int step{1};
    /// A choice's options; its owner moves `step` places among them for Left, Right or a pick.
    std::vector<std::string> choices;
    std::size_t chosen{0};
    /// For an action that throws something away: the question asked before it runs.
    std::string confirm;

    bool operator==(const SettingsRow&) const = default;
};

/// A row of `kind` with its label, note and value.
[[nodiscard]] SettingsRow makeRow(std::string id, RowKind kind, std::string label, std::string note,
                                  std::string value);
/// A switch row.
[[nodiscard]] SettingsRow makeToggle(std::string id, std::string label, std::string note, bool on);
/// Where a slider stands and how far it goes.
struct SliderRange {
    int level{};
    int low{};
    int high{};
    int step{1};
};
/// A slider row over `range`.
[[nodiscard]] SettingsRow makeSlider(std::string id, std::string label, std::string note,
                                     SliderRange range);
/// The options of a choice and the one chosen.
struct Options {
    std::vector<std::string> labels;
    std::size_t chosen{};
};
/// A choice row; its value reads the chosen option.
[[nodiscard]] SettingsRow makeChoice(std::string id, std::string label, std::string note,
                                     Options options);
/// A heading over the rows after it.
[[nodiscard]] SettingsRow makeHeading(std::string label);
/// A row opening the page `id`, reading `value`.
[[nodiscard]] SettingsRow makePage(std::string id, std::string label, std::string note,
                                   std::string value);

struct SettingsCategory {
    std::string id;
    std::string label;
    std::vector<SettingsRow> rows;

    bool operator==(const SettingsCategory&) const = default;
};

} // namespace opensu::ui

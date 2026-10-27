// setting_row — a settings-like row as its owner describes it: what kind it is, its words and its
// value. Every toolkit screen of rows turns each into the widget of its kind (`row_widgets`).
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
    /// For an action that throws something away: what the question asked before it runs says.
    std::string confirm;
    /// The question's button; empty for the label's first word ("Remove").
    std::string confirmVerb;
    /// What X does to the row, as its hint names it ("Forget"); empty when nothing.
    std::string secondary;

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

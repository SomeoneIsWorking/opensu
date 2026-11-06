// shelf_options — what shapes the shelf: Library's layout, the icon size, the pin, the sort, the
// source and the installed and hidden filters. Settings and the Guide show these rows; a change is
// kept at once and reaches the shelf behind.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "library/library_query.hpp"
#include "library/sections.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

inline constexpr std::string_view optionLayoutId = "layout";
inline constexpr std::string_view optionIconSizeId = "iconSize";
inline constexpr std::string_view optionPinId = "pinDock";
inline constexpr std::string_view optionSortId = "sort";
inline constexpr std::string_view optionSourceId = "source";
inline constexpr std::string_view optionInstalledId = "installedOnly";
inline constexpr std::string_view optionHiddenId = "hiddenOnly";

class ShelfOptions {
  public:
    /// What a change asks of the shell app.
    struct Hooks {
        /// Gives the screen the layout, pin and icon size the settings now hold.
        std::function<void()> applyLayout;
        /// Shows the shelf again, after the layout or the view changed.
        std::function<void()> reshow;
        /// The sources the source row steps through.
        std::function<std::vector<library::SourceChoice>()> sourceChoices;
    };

    ShelfOptions(Preferences& preferences, Hooks hooks)
        : preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    // Each row, `note` under its label.
    [[nodiscard]] ui::SettingsRow layout(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow iconSize(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow pin(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow sort(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow source(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow installedOnly(std::string note = {}) const;
    [[nodiscard]] ui::SettingsRow hiddenOnly(std::string note = {}) const;

    /// The rows that do something on `section`: Home has no layout, and its dock stays up.
    [[nodiscard]] std::vector<ui::SettingsRow> rowsFor(library::Section section) const;

    /// Acts on row `id`, `step` places on; false when it is not one of these rows.
    bool run(const std::string& id, int step);
    /// Sets the slider `id` to `level`; false when it is not one of these rows.
    bool setLevel(const std::string& id, int level);

  private:
    /// Keeps a change that moved or narrowed what the shelf shows.
    void shelfChanged();
    /// Keeps a change the screen takes from the settings.
    void lookChanged();

    Preferences& preferences_;
    Hooks hooks_;
};

} // namespace opensu::app

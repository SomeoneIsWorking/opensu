// layout_picker — START: the Library options panel. The three layout cards (Standard, XMB,
// Carousel), the icon size slider and the "Pin navigation bar" option under them, then openSU's
// own sort, source and installed/hidden filters, a way into the search and one into the Settings
// screen (iiSU's START menu holds its Settings the same way, screens.md §3.2). On Home the panel
// holds the rows that apply there. Every change is kept at once.
//
// iiSU offers the cards in its "Customize Games" menu (`roms_layout_cards`, navigation.md §2.1)
// with the icon size slider (`rom_browser_xmb_icon_size`, §2.3) and the pin option; its Global
// Search is an entry of the same START menu (screens.md `g73.b`). The sort and the filters are not
// iiSU's.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "library/library_query.hpp"
#include "library/sections.hpp"
#include "preferences.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

/// The ids of the panel's rows.
inline constexpr std::string_view optionIconSizeId = "options.iconSize";
inline constexpr std::string_view optionPinId = "options.pin";
inline constexpr std::string_view optionSortId = "options.sort";
inline constexpr std::string_view optionSourceId = "options.source";
inline constexpr std::string_view optionInstalledId = "options.installed";
inline constexpr std::string_view optionHiddenId = "options.hidden";
inline constexpr std::string_view optionSearchId = "options.search";
inline constexpr std::string_view optionSettingsId = "options.settings";

/// The id of the layout card for `mode`.
[[nodiscard]] std::string layoutCardId(library::LibraryMode mode);

class LayoutPicker final : public ui::RowActions {
  public:
    /// What the picker asks of the shell app.
    struct Hooks {
        /// Shows the shelf again, focusing `focus`, after the layout or the view changed.
        std::function<void(std::size_t focus)> reshow;
        /// The tile the grid has focused.
        std::function<std::size_t()> focusIndex;
        /// Gives the screen the layout, pin and icon size the settings now hold.
        std::function<void()> applyLayout;
        /// The sources the source filter steps through.
        std::function<std::vector<library::SourceChoice>()> sourceChoices;
        /// Opens the search panel.
        std::function<void()> openSearch;
        /// Opens the Settings screen.
        std::function<void()> openSettings;
    };

    /// The panel goes on `navigator`'s stack.
    LayoutPicker(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                 Preferences& preferences, Hooks hooks)
        : navigator_{navigator}, sounds_{sounds}, preferences_{preferences},
          hooks_{std::move(hooks)} {
    }

    /// Opens the panel; `inLibrary` is whether the layout cards, icon size and pin show.
    void open(bool inLibrary);
    /// Closes the panel with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;

    // What the screen asks.
    [[nodiscard]] bool inLibrary() const noexcept {
        return inLibrary_;
    }
    /// Library's layout now.
    [[nodiscard]] library::LibraryMode mode() const noexcept {
        return preferences_.values().libraryMode;
    }
    /// A layout card pressed: makes `mode` Library's layout, keeps it and closes the panel.
    void chooseMode(library::LibraryMode mode);
    /// The rows under the cards, as they stand now.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;
    void openPage(const std::string& id) override;

  private:
    /// The panel on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* panel() const;
    /// A change to what the library shows: keep it, show the shelf from its start.
    void viewChanged();

    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    Hooks hooks_;
    bool inLibrary_{false};
};

} // namespace opensu::app

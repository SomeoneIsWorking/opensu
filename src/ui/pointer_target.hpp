// pointer_target — what the pointer is on, as the shell's own elements name it.
#pragma once

#include <cstddef>
#include <variant>

#include "gamepad/event.hpp"
#include "library/game.hpp"
#include "library/sections.hpp"
#include "mode_chooser.hpp"

namespace opensu::ui {

/// A dock item.
struct OnDock {
    library::Section section;
    bool operator==(const OnDock&) const = default;
};

/// A home grid slot or a rail tile, by index.
struct OnTile {
    std::size_t index;
    bool operator==(const OnTile&) const = default;
};

/// A page arrow or a page dot, by the page it turns to.
struct OnPage {
    int page;
    bool operator==(const OnPage&) const = default;
};

/// A card of the Library layout picker.
struct OnLayoutCard {
    library::LibraryMode mode;
    bool operator==(const OnLayoutCard&) const = default;
};

/// An entry of the Guide menu.
struct OnGuideEntry {
    std::size_t index;
    bool operator==(const OnGuideEntry&) const = default;
};

/// The Guide menu's power button.
struct OnGuidePower {
    bool operator==(const OnGuidePower&) const = default;
};

/// A row of the Library options under the cards: pin, sort, source, filters or search.
struct OnChooserRow {
    ChooserRow row;
    bool operator==(const OnChooserRow&) const = default;
};

/// A point on the icon size slider, by the level it stands for.
struct OnIconSize {
    int level;
    bool operator==(const OnIconSize&) const = default;
};

/// A key of the search panel's keyboard, by its index in `searchKeys()`.
struct OnSearchKey {
    std::size_t index;
    bool operator==(const OnSearchKey&) const = default;
};

/// A listed result of the search panel, by its index in the results.
struct OnSearchResult {
    std::size_t index;
    bool operator==(const OnSearchResult&) const = default;
};

/// A row of the context menu.
struct OnContextItem {
    std::size_t index;
    bool operator==(const OnContextItem&) const = default;
};

/// The screen outside the open context menu, Guide menu or quick menu.
struct OnBackdrop {
    bool operator==(const OnBackdrop&) const = default;
};

/// A launcher's badge in the top bar.
struct OnLauncher {
    library::Source source;
    bool operator==(const OnLauncher&) const = default;
};

/// A hint on the launch panel, which presses the button it names.
struct OnPanelButton {
    gamepad::Button button;
    bool operator==(const OnPanelButton&) const = default;
};

/// A level of the breadcrumb trail, by its place in it.
struct OnCrumb {
    std::size_t index;
    bool operator==(const OnCrumb&) const = default;
};

/// A button of the details page, by its place on it.
struct OnDetailsButton {
    std::size_t index;
    bool operator==(const OnDetailsButton&) const = default;
};

/// A row of the quick menu, by its place in it.
struct OnQuickRow {
    std::size_t index;
    bool operator==(const OnQuickRow&) const = default;
};

/// A point on a quick menu slider's track, by the row and the level it stands for.
struct OnQuickSlider {
    std::size_t row;
    int level;
    bool operator==(const OnQuickSlider&) const = default;
};

/// A category of the Settings screen or the Devices page, by its place in the list.
struct OnSettingsCategory {
    std::size_t index;
    bool operator==(const OnSettingsCategory&) const = default;
};

/// A row of the focused category of the open page, by its place in it.
struct OnSettingsRow {
    std::size_t index;
    bool operator==(const OnSettingsRow&) const = default;
};

/// A point on a page slider's track, by the row and the level it stands for.
struct OnSettingsSlider {
    std::size_t row;
    int level;
    bool operator==(const OnSettingsSlider&) const = default;
};

/// An entry of the folder chooser, by its place in the list.
struct OnFolderEntry {
    std::size_t index;
    bool operator==(const OnFolderEntry&) const = default;
};

/// Nothing, or the element the pointer is on.
using PointerTarget =
    std::variant<std::monostate, OnDock, OnTile, OnPage, OnLayoutCard, OnGuideEntry, OnGuidePower,
                 OnPanelButton, OnChooserRow, OnIconSize, OnSearchKey, OnSearchResult,
                 OnContextItem, OnBackdrop, OnLauncher, OnCrumb, OnDetailsButton,
                 OnSettingsCategory, OnSettingsRow, OnSettingsSlider, OnFolderEntry, OnQuickRow,
                 OnQuickSlider>;

} // namespace opensu::ui

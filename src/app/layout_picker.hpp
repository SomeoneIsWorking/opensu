// layout_picker — START: the Library options panel's buttons. The layout cards, the icon size, the
// "Pin navigation bar" option, the sort, the source and installed/hidden filters and the way into
// the search and the Settings screen, and keeping what the player picks for the next run.
#pragma once

#include <functional>
#include <vector>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "library/library_query.hpp"
#include "preferences.hpp"
#include "ui/mode_chooser.hpp"

namespace opensu::app {

class LayoutPicker {
  public:
    /// What the picker asks of the shell app.
    struct Hooks {
        /// Shows the shelf again, focusing `focus`, after the layout or the view changed.
        std::function<void(std::size_t focus)> reshow;
        /// The tile the grid has focused.
        std::function<std::size_t()> focusIndex;
        /// Gives the screen the layout, pin and icon size the settings now hold.
        std::function<void()> applyLayout;
        /// The sources the source filter cycles through.
        std::function<std::vector<library::SourceChoice>()> sourceChoices;
        /// Opens the search panel.
        std::function<void()> openSearch;
        /// Opens the Settings screen.
        std::function<void()> openSettings;
    };

    LayoutPicker(ui::ModeChooser& chooser, audio::SoundPlayer& sounds, Preferences& preferences,
                 Hooks hooks)
        : chooser_{chooser}, sounds_{sounds}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the panel on the settings in use; `inLibrary` is whether the layout rows show.
    void open(bool inLibrary);
    /// A button while the panel is up: it takes them all.
    void act(gamepad::Button button);
    /// A click on the icon size slider.
    void chooseIconSize(int level);

  private:
    void close();
    /// Makes `mode` Library's layout and keeps it.
    void chooseMode(library::LibraryMode mode);
    /// Flips whether the dock stays up on Library and keeps it.
    void togglePin();
    /// Moves the icon size by `delta` levels.
    void stepIconSize(int delta);
    void cycleSort(int delta);
    void cycleSource(int delta);
    void toggleInstalled();
    void toggleHidden();
    /// A change to what the library shows: keep it, show the shelf from its start.
    void viewChanged();
    /// What the panel reads, from the settings.
    void syncValues();
    /// Acts on the focused row for A, or for a step along it for Left and Right (`step` is -1 or
    /// 1; 0 for A).
    void actOnRow(int step);

    ui::ModeChooser& chooser_;
    audio::SoundPlayer& sounds_;
    Preferences& preferences_;
    Hooks hooks_;
    /// The icon size changed since it was saved; it is kept when the panel closes.
    bool iconSizeDirty_{false};
};

} // namespace opensu::app

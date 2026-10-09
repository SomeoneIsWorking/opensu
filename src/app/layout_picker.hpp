// layout_picker — START on Library: the layout cards and the "Pin navigation bar" option, and
// keeping what the player picks for the next run.
#pragma once

#include <functional>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "settings/settings.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class LayoutPicker {
  public:
    /// `reshow` shows the shelf again after the layout changed.
    LayoutPicker(ui::Shell& shell, audio::SoundPlayer& sounds, const settings::Store& store,
                 settings::Settings& preferences, std::function<void()> reshow)
        : shell_{shell}, sounds_{sounds}, store_{store}, preferences_{preferences},
          reshow_{std::move(reshow)} {
    }

    /// Opens the picker on the layout and pin option in use.
    void open();
    /// A button while the picker is up: it takes them all.
    void act(gamepad::Button button);

  private:
    /// Makes `mode` Library's layout and keeps it.
    void chooseMode(library::LibraryMode mode);
    /// Flips whether the dock stays up on Library and keeps it.
    void togglePin();
    void save();

    ui::Shell& shell_;
    audio::SoundPlayer& sounds_;
    const settings::Store& store_;
    settings::Settings& preferences_;
    std::function<void()> reshow_;
};

} // namespace opensu::app

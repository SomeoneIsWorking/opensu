// context_menu_controller — the menu a focused tile opens (iiSU: SELECT on a ROM, `pb0.java:2086`;
// right click on a tile) and what its entries do. The tile it is open for is kept, so an entry
// acts on it whatever the grid does meanwhile.
#pragma once

#include <functional>
#include <optional>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "library/shelf.hpp"
#include "preferences.hpp"
#include "ui/context_menu.hpp"

namespace opensu::app {

class ContextMenuController {
  public:
    /// What the entries do, in the shell app's terms.
    struct Hooks {
        /// The tile the grid has focused, or null when none.
        std::function<const library::ShelfItem*()> focused;
        /// Launches the focused game, or offers to install it.
        std::function<void()> launch;
        std::function<void()> details;
        /// Hides or shows `game`, and shows the shelf again.
        std::function<void(const library::Game& game, bool hidden)> setHidden;
        std::function<void(const library::Folder& folder)> open;
        std::function<void()> refresh;
        std::function<void(library::Source source)> signIn;
    };

    ContextMenuController(ui::ContextMenu& menu, audio::SoundPlayer& sounds,
                          const Preferences& preferences, Hooks hooks)
        : menu_{menu}, sounds_{sounds}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the menu for the focused tile; nothing when no tile has focus.
    void open();
    /// A button while the menu is up: it takes them all.
    void act(gamepad::Button button);
    /// Closes the menu with iiSU's Close sound.
    void close();

  private:
    /// Does what entry `action` says to the tile the menu was opened for.
    void run(ui::ContextAction action);

    ui::ContextMenu& menu_;
    audio::SoundPlayer& sounds_;
    const Preferences& preferences_;
    Hooks hooks_;
    /// The tile the menu is open for.
    std::optional<library::ShelfItem> target_;
};

} // namespace opensu::app

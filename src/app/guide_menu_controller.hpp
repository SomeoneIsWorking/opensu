// guide_menu_controller — what the Guide menu does. Builds its entries for where the shell is (the
// sections, or the running game), moves its focus, and sends a chosen entry to the shell app's
// navigation, to power or to session mode: sleep, restart and shut down go through `host::Power`,
// switching to session mode through `host::SessionMode`; restart, shut down and the switch to
// session mode need a second press.
#pragma once

#include <functional>
#include <string>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "host/power.hpp"
#include "host/session_mode.hpp"
#include "library/sections.hpp"
#include "ui/guide_menu.hpp"

namespace opensu::app {

class GuideMenuController {
  public:
    /// What an entry asks of the shell app.
    struct Hooks {
        /// What the menu opens over: a running game, or the home screen.
        std::function<ui::GuideContext()> context;
        std::function<void(library::Section)> showSection;
        std::function<void()> openDevices;
        std::function<void()> openSettings;
        /// Ends the running game.
        std::function<void()> closeGame;
        /// Closes openSU, back to the desktop.
        std::function<void()> quit;
        /// Opens the Install session mode dialog.
        std::function<void()> installSession;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    GuideMenuController(ui::GuideMenu& menu, host::Power& power, host::SessionMode& session,
                        audio::SoundPlayer& sounds, Hooks hooks)
        : menu_{menu}, power_{power}, session_{session}, sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    /// Opens the menu over what the hooks say it is over.
    void open();
    /// Closes the menu with iiSU's Close sound.
    void close();
    /// Opens a closed menu and closes an open one.
    void toggle();
    /// A button while the menu is up.
    void act(gamepad::Button button);

  private:
    void choose();
    void perform(host::PowerAction action);
    /// Tells the player why a switch of sessions was refused; `what` names it.
    void report(const std::string& refused, const char* what);

    ui::GuideMenu& menu_;
    host::Power& power_;
    host::SessionMode& session_;
    audio::SoundPlayer& sounds_;
    Hooks hooks_;
};

} // namespace opensu::app

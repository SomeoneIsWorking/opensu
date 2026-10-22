// details_controller — the details page's buttons and what they do: play or install the game,
// update it, pick its emulator, open its settings tabs (`GameSettingsPage`) or clear the settings
// it keeps of its own,
// download the emulator it is missing, give a Windows install its own Wine prefix or the shared
// one, hide it, uninstall it (two presses), open its button mapping, go back. Opens for the focused
// game and keeps what it shows current while the library changes under it.
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "audio/sdl_sound_player.hpp"
#include "emulator_downloads.hpp"
#include "game_settings.hpp"
#include "gamepad/event.hpp"
#include "library/game.hpp"
#include "preferences.hpp"
#include "ui/details_page.hpp"
#include "uninstall_prompt.hpp"
#include "update_request.hpp"

namespace opensu::app {

class DetailsController {
  public:
    /// What the buttons do, in the shell app's terms.
    struct Hooks {
        /// The game the grid has focused, or null when none.
        std::function<const library::Game*()> focused;
        /// The game with this id as the grid shows it now, or nothing when it is gone.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Launches `game`, or offers to install it.
        std::function<void(const library::Game& game)> launch;
        /// Hides or shows `game`, and shows the shelf again.
        std::function<void(const library::Game& game, bool hidden)> setHidden;
        /// Runs ROM `game` on emulator `name` from now on; empty forgets its own pick.
        std::function<void(const library::Game& game, const std::string& name)> chooseEmulator;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Opens `game`'s button mapping over the page.
        std::function<void(const library::Game& game)> controls;
        /// Opens `game`'s settings tabs over the page.
        std::function<void(const library::Game& game)> openGameSettings;
        /// Whether frame generation can be set here (`FrameGeneration::available`).
        std::function<bool()> frameGeneration;
    };

    DetailsController(ui::DetailsPage& page, audio::SdlSoundPlayer& sounds,
                      Preferences& preferences, GameSettings& gameSettings,
                      EmulatorDownloads& downloads, UninstallPrompt& uninstall,
                      UpdateRequest& update, Hooks hooks)
        : page_{page}, sounds_{sounds}, preferences_{preferences}, gameSettings_{gameSettings},
          downloads_{downloads}, uninstall_{uninstall}, update_{update}, hooks_{std::move(hooks)} {
    }

    /// Opens the page for the focused game; nothing when no game has focus.
    void open();
    /// Closes the page with iiSU's Close sound.
    void close();
    /// A button while the page is up: it takes them all.
    void act(gamepad::Button button);
    /// Shows the game again as the grid has it now; the page closes when the game is gone.
    void refresh();

  private:
    [[nodiscard]] ui::DetailsView viewOf(const library::Game& game) const;
    /// The game's emulator source, on a built-in core its video settings, and the Game settings
    /// button when the tabs have anything for it.
    [[nodiscard]] ui::GameSettingsView settingsOf(const library::Game& game) const;
    void run(ui::DetailsAction action);
    /// Picks the emulator `delta` places along the game's list, wrapping.
    void cycleEmulator(const library::Game& game, int delta);
    /// Forgets every setting the game keeps of its own.
    void clearSettings(const library::Game& game);
    /// Starts downloading the emulator `game` is missing, or cancels its download.
    void downloadEmulator(const library::Game& game);
    /// Moves a Windows install between its own Wine prefix and the shared one.
    void toggleOwnPrefix(const library::Game& game);
    /// Arms Uninstall with what it removes, or on the second press starts the uninstall.
    void uninstall(const library::Game& game);

    ui::DetailsPage& page_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    GameSettings& gameSettings_;
    EmulatorDownloads& downloads_;
    UninstallPrompt& uninstall_;
    UpdateRequest& update_;
    Hooks hooks_;
};

} // namespace opensu::app

// details_controller — a game's details page and what its rows do: play or install the game,
// each store copy from its own button, update it, pick its emulator, open its settings
// (`LayeredEditor`), download the emulator it is missing, give a Windows install its own Wine
// prefix or the shared one, add it to Home or remove it, hide it, uninstall it (asking first),
// rename, change the file of or remove a game the player added
// (`OwnGamesController`), choose its cover, open its button mapping, copy its saves between
// emulators (`SaveCopyController`), sync them with paired openSUs (`SharedSaves`), say where Save
// and close left it and Start fresh without that (asking first), go back. Opens
// for the focused game and keeps what it shows current while the library changes under it.
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "emulator_downloads.hpp"
#include "game_settings.hpp"
#include "library/game.hpp"
#include "preferences.hpp"
#include "retro/resume_file.hpp"
#include "shared_saves.hpp"
#include "ui/game_details.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "uninstall_prompt.hpp"
#include "update_request.hpp"

namespace opensu::app {

class DetailsController final : public ui::RowActions {
  public:
    /// What the rows do, in the shell app's terms.
    struct Hooks {
        /// The game the grid has focused, or null when none.
        std::function<const library::Game*()> focused;
        /// The game with this id, or the tile of its title, as the grid shows it now, or nothing
        /// when it is gone.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Launches `game`, or offers to install it.
        std::function<void(const library::Game& game)> launch;
        /// Hides or shows `game`, and shows the shelf again.
        std::function<void(const library::Game& game, bool hidden)> setHidden;
        /// Whether `game` is on Home.
        std::function<bool(const library::Game& game)> onHome;
        /// Whether `game`, running on another openSU, has a seat of the profile played here.
        std::function<bool(const library::Game& game)> continues;
        /// Puts `game` on Home or takes it off, and shows the shelf again.
        std::function<void(const library::Game& game, bool on)> setOnHome;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Opens `game`'s button mapping over the page.
        std::function<void(const library::Game& game)> controls;
        /// Opens `game`'s settings over the page, with `rows` on the game's own tab.
        std::function<void(const library::Game& game,
                           std::function<std::vector<ui::SettingsRow>()> rows)>
            openGameSettings;
        /// How many settings `game` keeps of its own; nothing when it has nothing to set.
        std::function<std::optional<std::size_t>(const library::Game& game)> gameSettings;
        /// Opens "Choose artwork" for `game` over the page.
        std::function<void(const library::Game& game)> chooseArtwork;
        /// A game the player added: asks for its new title, its new file, or whether to remove it.
        std::function<void(const library::Game& game)> rename;
        std::function<void(const library::Game& game)> changeFile;
        std::function<void(const library::Game& game)> remove;
        /// Whether game `id`'s cover is downloading or being fitted.
        std::function<bool(const std::string& id)> artDownloading;
        /// Opens Match a different game for a game the player added.
        std::function<void(const library::Game& game)> matchGame;
        /// Reads `game`'s saves again off the frame, for the Copy saves row and the shared saves'
        /// fact; `refresh` shows them.
        std::function<void(const library::Game& game)> readSaves;
        /// The emulators keeping a save of `game`, as Copy saves reads them; empty for no row.
        std::function<std::string(const library::Game& game)> copySavesOffer;
        /// Opens Copy saves for `game` over the page.
        std::function<void(const library::Game& game)> copySaves;
        /// How `game`'s saves stand with the openSUs sharing them, and the row that syncs them.
        std::function<SharedSaves::View(const library::Game& game)> sharedSaves;
        /// That row: syncs now, or asks which save to keep.
        std::function<void(const library::Game& game)> syncSaves;
        /// The header of the resume state Save and close left of `game`, or nothing.
        std::function<std::optional<retro::ResumeHeader>(const library::Game& game)> resume;
        /// Throws that state away: empty when it is gone, else why not.
        std::function<std::string(const library::Game& game)> startFresh;
        /// The paired openSUs a linked game can be played with now.
        std::function<std::vector<ui::GameSettingsView::Partner>()> partners;
        /// Asks the paired openSU `peerId` to play `game` linked.
        std::function<void(const library::Game& game, const std::string& peerId)> playWith;
        /// Offers `game` to a friend over the internet.
        std::function<void(const library::Game& game)> playOnline;
        /// The store copies with rows of their own on `game`'s page, top first; empty for a page
        /// of `game` alone.
        std::function<std::vector<library::Game>(const library::Game& game)> copies;
    };

    /// What the rows act on.
    struct Services {
        Preferences& preferences;
        GameSettings& gameSettings;
        EmulatorDownloads& downloads;
        UninstallPrompt& uninstall;
        UpdateRequest& update;
    };

    /// Where the page goes: the stack, the chrome it leaves room for and the pictures it shows.
    struct Place {
        gpu2d::ui::Navigator& navigator;
        const ui::Chrome& chrome;
        ui::ScreenImages& images;
    };

    DetailsController(Place place, audio::SdlSoundPlayer& sounds, Services services, Hooks hooks)
        : navigator_{place.navigator}, chrome_{place.chrome}, images_{place.images},
          sounds_{sounds}, preferences_{services.preferences}, gameSettings_{services.gameSettings},
          downloads_{services.downloads}, uninstall_{services.uninstall}, update_{services.update},
          hooks_{std::move(hooks)} {
    }

    /// Opens the page for the focused game; nothing when no game has focus.
    void open();
    /// Closes the page and what is over it with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Shows the game again as the grid has it now; the page closes when the game is gone.
    void refresh();
    /// What the page shows; null while it is closed.
    [[nodiscard]] const ui::GameDetails* details() const;
    /// Whether the page's cover is on its way, which its spinner shows.
    [[nodiscard]] bool artDownloading() const;

    // What the page asks.
    /// The launch button of game `id`, the page's or a copy of it: Play, or offer to install.
    void launch(const std::string& id);
    void run(const std::string& id, int step) override;
    void openPage(const std::string& id) override;

  private:
    /// The page on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* page() const;
    [[nodiscard]] ui::GameDetails detailsOf(const library::Game& game) const;
    /// The game's emulator source, on a built-in core its video settings, and what the Game
    /// settings row reads when the game has anything to set.
    [[nodiscard]] ui::GameSettingsView settingsOf(const library::Game& game) const;
    /// The page's game as the grid shows it now; the page closes when it is gone.
    [[nodiscard]] std::optional<library::Game> current();
    /// The game a row or button names: the page's when `gameId` is empty, else that copy of it;
    /// nothing when it is gone.
    [[nodiscard]] std::optional<library::Game> rowGame(const std::string& gameId) const;
    /// The copies with a button, in the order the page opened with.
    [[nodiscard]] std::vector<library::Game> copiesOf(const library::Game& game) const;
    /// Picks the Emulator choice's option `step` places on, wrapping.
    /// Starts downloading the emulator `game` is missing, or cancels its download.
    void downloadEmulator(const library::Game& game);
    /// Moves a Windows install between its own Wine prefix and the shared one.
    void toggleOwnPrefix(const library::Game& game);
    /// Asks before uninstalling `game`, saying what goes, or says why it cannot be.
    void uninstall(const library::Game& game);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    GameSettings& gameSettings_;
    EmulatorDownloads& downloads_;
    UninstallPrompt& uninstall_;
    UpdateRequest& update_;
    Hooks hooks_;
    /// What the page shows, as last read.
    std::optional<ui::GameDetails> details_;
    /// The copies' ids as the page opened: buttons never swap under a pointer while it is up.
    std::vector<std::string> openOrder_;
};

} // namespace opensu::app

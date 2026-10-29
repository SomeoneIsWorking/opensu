// own_games_controller — adding and keeping the player's own games (`library::own`): Add a game
// offers a file, picked on the folder chooser and identified on a worker (`library::identify`:
// title, platform, Steam app), which the player confirms or retitles before it is added, and every
// Steam shortcut and Lutris game not imported yet; a game's details rename it, point it at another
// file, or remove it from the library after asking, its files left alone.
#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "config/config.hpp"
#include "keyboard_pad.hpp"
#include "library/game_identify.hpp"
#include "library/own_games.hpp"
#include "path_editor.hpp"

namespace opensu::app {

class OwnGamesController {
  public:
    /// What the controller shows its screens with.
    struct Parts {
        gpu2d::ui::Navigator& navigator;
        PathEditor& paths;
        KeyboardPad& keyboard;
        audio::SdlSoundPlayer& sounds;
    };

    /// Where the entries are kept and where imports are read from.
    struct Places {
        /// The entries' file (`PlayerFiles::ownGamesFile`).
        std::filesystem::path records;
        /// Where the file chooser opens: `~/Games` when there is one, else here.
        std::filesystem::path home;
        /// Steam's install roots (`steam::Library::roots`).
        std::vector<std::filesystem::path> steamRoots;
        /// Lutris's data folder (`lutris::dataDir`).
        std::filesystem::path lutrisData;

        /// Where they are for `config`, the entries kept in `records`.
        [[nodiscard]] static Places of(const config::Config& config, std::filesystem::path records);
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The entries changed: the library is read again.
        std::function<void()> changed;
    };

    OwnGamesController(Parts parts, Places places, Hooks hooks);

    /// Add a game: a file, or the imports there are.
    void openAdd();
    /// Shows a finished identification for confirming. Main loop only.
    void service();
    /// Picks the file of a new game.
    void chooseFile();
    /// Adds every Steam shortcut, or Lutris game, not imported yet.
    void importSteamShortcuts();
    void importLutris();

    /// From a game's details.
    void rename(const library::Game& game);
    void changeFile(const library::Game& game);
    void remove(const library::Game& game);

  private:
    /// The entries of `found` not imported yet.
    [[nodiscard]] std::vector<library::own::Entry>
    notImported(std::vector<library::own::Entry> found) const;
    /// Steam's shortcuts and Lutris's games; empty when they cannot be read, which is logged.
    [[nodiscard]] std::vector<library::own::Entry> steamShortcuts() const;
    [[nodiscard]] std::vector<library::own::Entry> lutrisGames() const;
    void importAll(std::vector<library::own::Entry> entries, std::string_view from);
    /// Opens the folder chooser on `start` for a game file; `picked` takes the one accepted.
    template <typename Picked>
    void pickFile(const std::string& title, const std::filesystem::path& start, Picked picked);
    /// Asks for a title over the screen, `title` typed in already and `note` under the heading;
    /// `done` takes what was entered.
    void askTitle(const std::string& heading, const std::string& note, std::string title,
                  std::function<void(const std::string&)> done);
    /// Identifies `file` on the worker; `service` shows the guess.
    void identify(const std::filesystem::path& file);
    /// The confirm card of a guess.
    void confirm(const std::filesystem::path& file, const library::identify::Guess& guess);
    /// Closes the title card.
    void closeTitle();
    /// Runs `change` on the records and reloads the library; a failure is said, not thrown.
    void apply(const std::function<void()>& change, const std::string& done);

    gpu2d::ui::Navigator& navigator_;
    PathEditor& paths_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Places places_;
    Hooks hooks_;
    library::own::Records records_;
    /// The title card's text and what takes it.
    std::string title_;
    std::function<void(const std::string&)> titled_;
    /// A finished identification, waiting for the main loop.
    std::mutex mutex_;
    std::optional<std::pair<std::filesystem::path, library::identify::Guess>> identified_;
    /// Last, so it stops before anything it writes is destroyed.
    std::jthread identifier_;
};

} // namespace opensu::app

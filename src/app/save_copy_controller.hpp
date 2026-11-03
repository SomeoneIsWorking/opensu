// save_copy_controller — "Copy saves" over a game's details page: the installed emulators that keep
// a save of the game (`library::saves::savesOf`), the one to copy from and the one to copy to,
// each with its newest change and size, the copy with the target's saves backed up first
// (`SaveCopier`) and its undo. The details page offers it while two or more keep a save.
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "emulator_downloads.hpp"
#include "library/game.hpp"
#include "library/game_saves.hpp"
#include "library/save_copy.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class SaveCopyController final : public ui::RowActions {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// A copy or an undo changed the game's saves.
        std::function<void()> changed;
    };

    /// Where the saves are: the catalog of `downloads`, the built-in runner's folder
    /// (`Config::savesDir`) and the state dir the backups go under.
    struct Places {
        std::filesystem::path runnerSaves;
        std::filesystem::path stateDir;
    };

    SaveCopyController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome,
                       audio::SdlSoundPlayer& sounds, const EmulatorDownloads& downloads,
                       Places places, Hooks hooks)
        : navigator_{navigator}, chrome_{chrome}, sounds_{sounds}, downloads_{downloads},
          runnerSaves_{std::move(places.runnerSaves)}, copier_{std::move(places.stateDir)},
          hooks_{std::move(hooks)} {
    }

    /// The emulators keeping a save of `game`, as the details page's row reads them
    /// ("Eden, Ryujinx"); empty while fewer than two do.
    [[nodiscard]] std::string offer(const library::Game& game) const;

    /// Opens over the details page for `game`; nothing while fewer than two emulators keep a save.
    void open(const library::Game& game);
    void close();
    [[nodiscard]] bool isOpen() const;

    /// The open card's game title; empty while it is closed.
    [[nodiscard]] std::string gameTitle() const {
        return opened_ ? opened_->game.title : std::string{};
    }
    /// The card's rows: copy from, copy to, Copy saves, Undo copy while a backup is there, Close.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;

    void run(const std::string& id, int step) override;

  private:
    /// The game the card is open for and what its saves are found by.
    struct Opened {
        library::Game game;
        library::saves::SaveKey key;
    };

    [[nodiscard]] const gpu2d::ui::Screen* card() const;
    [[nodiscard]] std::vector<library::saves::EmulatorSaves>
    savesOf(const library::Game& game) const;
    /// Reads `game`'s saves again, keeping the source and target by emulator.
    void reread(const library::Game& game);
    /// The places other than the source, in order.
    [[nodiscard]] std::vector<std::size_t> targets() const;
    void copy(const Opened& opened);
    void undo(const Opened& opened);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    audio::SdlSoundPlayer& sounds_;
    const EmulatorDownloads& downloads_;
    std::filesystem::path runnerSaves_;
    library::saves::SaveCopier copier_;
    Hooks hooks_;
    std::optional<Opened> opened_;
    std::vector<library::saves::EmulatorSaves> saves_;
    std::size_t from_{0};
    std::size_t to_{1};
};

} // namespace opensu::app

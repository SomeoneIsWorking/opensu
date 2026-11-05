// save_copy_controller — "Copy saves" over a game's details page: the installed emulators that
// keep saves (`library::saves::savesOf`), the one to copy from (one holding a save of the game)
// and the one to copy to (any other, with a save of the game or none yet), each with its newest
// change and size, the copy with the target's saves backed up first (`SaveCopier`) and its undo.
// Every read, copy and undo runs on the worker, one at a time, refused while either emulator is
// running (`launch::ProcessTree`); the card's row shows how far a copy or undo is. The details
// page offers it once a read finds a save to copy and another emulator to take it.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "emulator_downloads.hpp"
#include "library/game.hpp"
#include "library/game_saves.hpp"
#include "library/save_copy.hpp"
#include "player_profiles.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class SaveCopyController final : public ui::RowActions {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// A read, a copy or an undo changed what the details page offers.
        std::function<void()> changed;
    };

    /// Where the saves are: the catalog of `downloads`, the active profile's (its built-in runner
    /// folder under `PlayerProfiles::runnerSaves`) and the state dir the backups go under.
    struct Places {
        const PlayerProfiles& profiles;
        std::filesystem::path stateDir;
    };

    SaveCopyController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome,
                       audio::SdlSoundPlayer& sounds, const EmulatorDownloads& downloads,
                       Places places, Hooks hooks)
        : navigator_{navigator}, chrome_{chrome}, sounds_{sounds}, downloads_{downloads},
          profiles_{places.profiles}, copier_{std::move(places.stateDir)},
          hooks_{std::move(hooks)} {
    }
    SaveCopyController(const SaveCopyController&) = delete;
    SaveCopyController& operator=(const SaveCopyController&) = delete;
    SaveCopyController(SaveCopyController&&) = delete;
    SaveCopyController& operator=(SaveCopyController&&) = delete;
    ~SaveCopyController() override = default;

    /// Reads `game`'s saves on the worker, for the details page's row; nothing while it is busy.
    void read(const library::Game& game);
    /// The emulators keeping saves of `game` as the last read found them ("Eden, Ryujinx"); empty
    /// while it reads, or when there is no save to copy or no emulator to take it.
    [[nodiscard]] std::string offer(const library::Game& game) const;

    /// Opens over the details page for `game`, with what the last read found; nothing when it
    /// offers no copy.
    void open(const library::Game& game);
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Takes what the worker finished. Main loop only.
    void service();

    /// The open card's game title; empty while it is closed.
    [[nodiscard]] std::string gameTitle() const {
        return opened_ ? opened_->game.title : std::string{};
    }
    /// The card's rows: copy from, copy to, Copy saves (or how far it is), Undo copy while a backup
    /// is there, Close.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;

    void run(const std::string& id, int step) override;

  private:
    /// The game the card is open for and what its saves are found by: the key, and the profile's
    /// seat (`ProfileBook::seatOf`).
    struct Opened {
        library::Game game;
        library::saves::SaveKey key;
        std::size_t seat{0};
    };
    /// What a read found: each emulator's saves and its newest backup.
    struct Found {
        std::string gameId;
        std::vector<library::saves::EmulatorSaves> saves;
        std::map<std::string, library::saves::SaveBackup, std::less<>> backups;
    };
    /// What the worker finished: a read, after a copy or an undo with what to tell the player.
    struct Done {
        Found found;
        std::string said;
        bool error{false};
    };
    enum class Work : std::uint8_t { None, Reading, Copying, Undoing };

    [[nodiscard]] const gpu2d::ui::Screen* card() const;
    /// Runs `job` on the worker; nothing while it is busy.
    template <std::invocable Job> void start(Work work, Job job);
    /// What a read of `game`'s saves through `catalog` finds. Worker only.
    [[nodiscard]] Found find(const Opened& opened,
                             const library::roms::EmulatorCatalog& catalog) const;
    /// What the active profile's saves of `game` are found by; nothing when openSU finds none.
    [[nodiscard]] std::optional<Opened> openedFor(const library::Game& game) const;
    /// The places that can be copied from: those holding a save.
    [[nodiscard]] std::vector<std::size_t> sources() const;
    /// The places other than the source, in order.
    [[nodiscard]] std::vector<std::size_t> targets() const;
    /// Points the source and target at `source` and `target` where they still are.
    void place(const std::string& source, const std::string& target);
    void copy(Opened opened);
    void undo(Opened opened);
    /// "Close Ryujinx first: it is running"; empty when neither of `saves` is running.
    [[nodiscard]] static std::string
    runningRefusal(std::initializer_list<const library::saves::EmulatorSaves*> saves);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    audio::SdlSoundPlayer& sounds_;
    const EmulatorDownloads& downloads_;
    const PlayerProfiles& profiles_;
    library::saves::SaveCopier copier_;
    Hooks hooks_;
    std::optional<Opened> opened_;
    /// What the last read found; the card and the details row read it.
    std::optional<Found> found_;
    /// The game being read for the details row.
    std::string reading_;
    std::size_t from_{0};
    std::size_t to_{1};
    Work work_{Work::None};

    library::saves::CopyProgress progress_;
    std::mutex mutex_;
    std::optional<Done> done_;
    /// Last, so it stops before anything it writes is destroyed.
    std::jthread worker_;
};

} // namespace opensu::app

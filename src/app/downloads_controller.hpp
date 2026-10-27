// downloads_controller — the Downloads screen's contents and buttons. Each frame it reads every
// download source (Steam's queue, the store install, the emulator download, the Wine runner
// download, the romset lists) into `DownloadsBoard`, keeps the rows the screen shows from it, and
// sends a chosen action to the row's owner: a cancel to `Installs`, `EmulatorDownloads` or
// `WineRunners`, a pause, resume, move or remove to Steam through `steam::DownloadControl`, which
// runs off the loop as Steam answers.
#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "control_channel.hpp"
#include "downloads_board.hpp"
#include "emulator_downloads.hpp"
#include "installs.hpp"
#include "library/romset_lists.hpp"
#include "steam/download_control.hpp"
#include "ui/screen_stack.hpp"
#include "wine_runners.hpp"

namespace opensu::app {

/// One download as the screen shows it.
struct DownloadsEntry {
    /// The board's name for the row, stable across refreshes.
    std::string key{};
    std::string title{};
    /// Where it comes from: "Steam", "Steam tool", "Epic", "GOG", "Emulator", "Core".
    std::string source{};
    /// "Downloading", "Paused", "Failed" and so on.
    std::string state{};
    /// The figures after the state: percent, bytes, rate, time left; or why it failed.
    std::string figures{};
    /// 0 to 1 when measured; the bar shows only then.
    std::optional<double> fraction{};
    bool failed{false};
    /// Over: shown quieter, with no buttons.
    bool ended{false};
    std::vector<std::string> actions{};

    bool operator==(const DownloadsEntry&) const = default;
};

/// The screen's text for one row.
[[nodiscard]] DownloadsEntry entryOf(const DownloadRow& row);

class DownloadsController {
  public:
    /// Where the downloads come from and go to.
    struct Sources {
        Installs& installs;
        EmulatorDownloads& emulators;
        WineRunners& runners;
        const library::roms::RomsetLists& romsetLists;
        steam::DownloadControl& steam;
        /// Whether Steam runs and can be asked.
        std::function<bool()> steamReady;
        /// Steam's queue as last read.
        std::function<std::vector<steam::Download>()> steamQueue;
        /// A game's title by store and id.
        DownloadsBoard::TitleOf titles;
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// A romset list fetch ended: the library is matched again.
        std::function<void()> romsetListsEnded;
    };

    /// The screen goes on `navigator`'s stack, under `chrome`.
    DownloadsController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome, Sources sources,
                        audio::SdlSoundPlayer& sounds, Hooks hooks);
    ~DownloadsController();
    DownloadsController(const DownloadsController&) = delete;
    DownloadsController& operator=(const DownloadsController&) = delete;

    /// Opens the screen.
    void open();
    /// Closes the screen with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Reads every source into the board, takes Steam's answers and keeps the rows current. Main
    /// loop only.
    void service();
    /// The rows as the screen shows them.
    [[nodiscard]] const std::vector<DownloadsEntry>& entries() const noexcept {
        return shown_;
    }
    /// Does button `action` of the row `key`.
    void press(const std::string& key, std::size_t action);

    [[nodiscard]] const DownloadsBoard& board() const noexcept {
        return board_;
    }
    /// A Steam call is out and its answer not yet taken.
    [[nodiscard]] bool answering() const noexcept {
        return asking_;
    }
    /// The rows for the control channel's `/state`.
    [[nodiscard]] std::vector<DownloadLine> lines() const;

  private:
    /// A Steam call that ran off the loop, and what came of it.
    struct SteamAnswer {
        std::string key{};
        DownloadAction action{DownloadAction::Pause};
        std::string refusal{};
    };

    /// The romset list fetches as the board takes them; tells the player of each that ended.
    [[nodiscard]] std::vector<RomsetListLook> lookAtRomsetLists();
    /// Does `action` to `row` through its owner.
    void perform(const DownloadRow& row, DownloadAction action);
    /// Cancels the store install of `row`, if it is the one that runs.
    void cancelInstall(const DownloadRow& row);
    /// Runs a Steam call on the worker; refused while one is still out.
    void askSteam(const DownloadRow& row, DownloadAction action);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    Sources sources_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    DownloadsBoard board_;
    std::vector<DownloadsEntry> shown_;
    std::mutex mutex_;
    std::optional<SteamAnswer> answer_;
    bool asking_{false};
    /// `RomsetLists::ended` as last seen, and the fetches then.
    std::uint64_t romsetListsEnded_{0};
    std::vector<library::roms::RomsetFetch> romsetFetches_;
    /// Last, so it is joined before what it uses goes.
    std::jthread worker_;
};

} // namespace opensu::app

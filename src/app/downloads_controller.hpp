// downloads_controller — the Downloads page's contents and buttons. Each frame it reads every
// download source (Steam's queue, the store install, the emulator download, the romset lists) into
// `DownloadsBoard`, shows the board's rows on the page while it is up, and sends a chosen action to
// the row's owner: a cancel to `Installs` or `EmulatorDownloads`, a pause, resume, move or remove
// to Steam through `steam::DownloadControl`, which runs off the loop as Steam answers.
#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "control_channel.hpp"
#include "downloads_board.hpp"
#include "emulator_downloads.hpp"
#include "gamepad/event.hpp"
#include "installs.hpp"
#include "library/romset_lists.hpp"
#include "steam/download_control.hpp"
#include "ui/downloads_page.hpp"

namespace opensu::app {

/// The page's text for one row.
[[nodiscard]] ui::DownloadsEntry entryOf(const DownloadRow& row);

class DownloadsController {
  public:
    /// Where the downloads come from and go to.
    struct Sources {
        Installs& installs;
        EmulatorDownloads& emulators;
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

    DownloadsController(ui::DownloadsPage& page, Sources sources, audio::SdlSoundPlayer& sounds,
                        Hooks hooks);
    ~DownloadsController();
    DownloadsController(const DownloadsController&) = delete;
    DownloadsController& operator=(const DownloadsController&) = delete;

    /// Opens the page on its first row.
    void open();
    /// Closes the page with iiSU's Close sound.
    void close();
    /// A button while the page is up.
    void act(gamepad::Button button);
    /// Reads every source into the board, takes Steam's answers and keeps the page current. Main
    /// loop only.
    void service();

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

    [[nodiscard]] std::vector<ui::DownloadsEntry> entries() const;
    /// The romset list fetches as the board takes them; tells the player of each that ended.
    [[nodiscard]] std::vector<RomsetListLook> lookAtRomsetLists();
    /// Does `action` to `row` through its owner.
    void perform(const DownloadRow& row, DownloadAction action);
    /// Cancels the store install of `row`, if it is the one that runs.
    void cancelInstall(const DownloadRow& row);
    /// Runs a Steam call on the worker; refused while one is still out.
    void askSteam(const DownloadRow& row, DownloadAction action);

    ui::DownloadsPage& page_;
    Sources sources_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    DownloadsBoard board_;
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

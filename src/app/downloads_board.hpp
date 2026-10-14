// downloads_board — every download openSU knows of as one list, the Downloads page's model: Steam's
// queue (its own auto-updates included), the store install that runs (Epic, GOG, Steam's
// installer), and the emulator or core download. Each frame the sources' state goes in and one row
// per download comes out, with its state, progress and the actions its owner supports. A
// download that ended stays listed until openSU quits. Pure; no UI and no threads.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "installs.hpp"
#include "library/game.hpp"
#include "steam/downloads.hpp"

namespace opensu::app {

enum class DownloadKind : std::uint8_t { Game, Emulator, Core };

enum class DownloadState : std::uint8_t {
    Queued,
    Downloading,
    Paused,
    /// Past the transfer: Steam verifying or writing, an emulator being checked and switched to.
    Installing,
    Finished,
    Failed,
    Cancelled,
};

enum class DownloadAction : std::uint8_t {
    /// Steam's queue as a whole, as Steam's own pause and resume button.
    Pause,
    Resume,
    /// Starts a waiting Steam download now.
    MoveToTop,
    MoveUp,
    MoveDown,
    /// Takes a Steam download off the queue.
    Remove,
    /// Stops an install or emulator download openSU runs.
    Cancel,
};

/// One download as the page lists it.
struct DownloadRow {
    /// `steam:<app id>`, `epic:<app name>`, `gog:<id>`, `emulator:<id>`: stable across frames.
    std::string key{};
    /// The store, for a game; nothing for an emulator or a core.
    std::optional<library::Source> store{};
    DownloadKind kind{DownloadKind::Game};
    /// The owner's id: the app id, the emulator's catalog id.
    std::string id{};
    std::string title{};
    DownloadState state{DownloadState::Queued};
    /// Steam says the active download is an update rather than a first install.
    bool update{false};
    /// 0 to 1, when the owner measures it.
    std::optional<double> fraction{};
    /// Bytes moved and to move; 0 when the owner does not say.
    std::uint64_t bytesDone{0};
    std::uint64_t bytesTotal{0};
    std::optional<std::uint64_t> bytesPerSecond{};
    std::optional<int> secondsLeft{};
    /// The owner's own line: an install's stage, or why it failed.
    std::string detail{};
    /// Steam lists it, at this place of its queue (0 is the head, -1 out of the queue).
    bool inSteamQueue{false};
    int queueIndex{-1};
    /// No queued Steam download comes after it.
    bool lastInQueue{false};

    bool operator==(const DownloadRow&) const = default;
};

/// What the row's owner lets the player do with it now.
[[nodiscard]] std::vector<DownloadAction> actionsFor(const DownloadRow& row);
[[nodiscard]] std::string_view label(DownloadAction action) noexcept;
[[nodiscard]] std::string_view label(DownloadState state) noexcept;
/// `queued`, `downloading` and so on, for the control channel.
[[nodiscard]] std::string_view spelling(DownloadState state) noexcept;
/// Whether the download is over.
[[nodiscard]] bool ended(DownloadState state) noexcept;

/// The emulator or core downloading now.
struct EmulatorLook {
    std::string id{};
    std::string name{};
    bool core{false};
    std::uint64_t received{0};
    /// 0 until the size is known.
    std::uint64_t total{0};
};

/// How the last emulator or core download ended.
struct EmulatorEnd {
    /// Counts ends, so one is applied once.
    std::uint64_t serial{0};
    std::string id{};
    std::string name{};
    bool core{false};
    DownloadState state{DownloadState::Finished};
    std::string text{};
};

class DownloadsBoard {
  public:
    /// A game's title by its store and the store's id; empty when the library has none.
    using TitleOf = std::function<std::string(library::Source, const std::string&)>;

    /// One frame's look at every source.
    struct Sources {
        /// Steam can be asked; its queue below is current.
        bool steamReady{false};
        std::vector<steam::Download> steam{};
        std::optional<Installs::Status> install{};
        std::optional<EmulatorLook> emulator{};
        std::optional<EmulatorEnd> emulatorEnded{};
    };

    explicit DownloadsBoard(TitleOf titles) : titles_{std::move(titles)} {
    }

    /// Merges the sources into the rows.
    void update(const Sources& sources);

    /// Live downloads first (downloading, paused, then queued in Steam's order), then the ended
    /// ones, latest first.
    [[nodiscard]] const std::vector<DownloadRow>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] const DownloadRow* find(std::string_view key) const;

    /// The player took the Steam download `key` off the queue: it leaves the list now and is not
    /// listed again while Steam still reports it.
    void removed(const std::string& key);

  private:
    struct Entry {
        DownloadRow row;
        /// When it was first seen and when it ended, in updates.
        std::uint64_t seen{0};
        std::uint64_t endedAt{0};
        /// The install or emulator end already applied.
        std::uint64_t endSerial{0};
        bool live{false};
    };

    /// The entry for `key`, made when missing.
    Entry& entry(const std::string& key);
    void applyInstall(const Installs::Status& install, std::set<std::string>& live);
    void applySteam(const std::vector<steam::Download>& queue, std::set<std::string>& live);
    void applyEmulator(const Sources& sources, std::set<std::string>& live);
    /// Settles what was live and is gone now.
    void settle(const std::set<std::string>& live, bool steamReady);
    void order();

    TitleOf titles_;
    std::map<std::string, Entry, std::less<>> entries_;
    /// Steam downloads the player removed that Steam may still list for a moment.
    std::set<std::string, std::less<>> removed_;
    std::vector<DownloadRow> rows_;
    std::uint64_t tick_{0};
};

} // namespace opensu::app

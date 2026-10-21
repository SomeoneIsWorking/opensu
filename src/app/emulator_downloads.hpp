// emulator_downloads — downloading, updating and removing the emulators openSU installs, off the
// main loop: official AppImages and built-in emulators' libretro cores, one download at a time
// with its progress and cancel, the update check, and the news the loop hands to the screens
// (`service`). Nothing updates unless the player asks.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "library/emulator_release.hpp"
#include "library/emulators.hpp"
#include "library/libretro_cores.hpp"
#include "net/web_client.hpp"

namespace opensu::app {

class EmulatorDownloads {
  public:
    /// What the loop is told.
    struct Hooks {
        /// A download ended: installed, cancelled or failed; `error` marks a failure.
        std::function<void(const std::string& text, bool error)> say;
        /// An emulator was installed, updated or removed: the library reads again.
        std::function<void()> installedChanged;
        /// Progress or an update check moved: the screens that show it refresh.
        std::function<void()> refresh;
    };

    /// Where the running download stands.
    struct Progress {
        std::string id;
        std::uint64_t received{0};
        /// 0 until the size is known.
        std::uint64_t total{0};

        /// 0 to 100; nothing while the size is unknown.
        [[nodiscard]] std::optional<int> percent() const noexcept;
    };

    /// One version of an emulator.
    struct Build {
        /// As the player reads it: a release's tag, a core's build date.
        std::string version;
        /// What tells two versions apart: a release asset's id, a core zip's CRC-32.
        std::string key;

        bool operator==(const Build&) const = default;
    };

    /// How a download ended, kept until the next one ends.
    struct Finished {
        enum class Outcome : std::uint8_t { Installed, Cancelled, Failed };
        /// Counts the ends, so each is told apart.
        std::uint64_t serial{0};
        std::string id;
        Outcome outcome{Outcome::Installed};
        /// What the player was told.
        std::string text;
    };

    /// The update check's state.
    struct Check {
        bool running{false};
        bool done{false};
        /// The emulators whose release could not be read, with why.
        std::vector<std::string> failures;
    };

    EmulatorDownloads(library::roms::EmulatorSetup setup, Hooks hooks);
    ~EmulatorDownloads();
    EmulatorDownloads(const EmulatorDownloads&) = delete;
    EmulatorDownloads& operator=(const EmulatorDownloads&) = delete;

    [[nodiscard]] const library::roms::EmulatorSetup& setup() const noexcept {
        return setup_;
    }
    /// Searches for the emulators now: only the built-in ones while standalone emulators are off.
    [[nodiscard]] library::roms::Emulators discover() const {
        return setup_.discover();
    }
    /// Turns standalone emulators on or off: off, they are neither searched for, downloaded nor
    /// checked for updates. Main loop only.
    void setStandalone(bool standalone) noexcept {
        setup_.standalone = standalone;
    }

    /// Downloads the latest release or core build of `id` and switches to it. Refused, with the
    /// reason, while another download runs or for an emulator openSU cannot download.
    [[nodiscard]] std::string start(std::string_view id);
    /// Stops the running download of `id`; the version in use stays.
    void cancel(std::string_view id);
    /// The running download, or nothing; a download runs until service() reports its end.
    [[nodiscard]] std::optional<Progress> progress() const;
    /// The download service() last reported the end of, or nothing before the first.
    [[nodiscard]] std::optional<Finished> lastFinished() const;

    /// Reads the latest release or core build of every downloadable emulator, in the background.
    void checkUpdates();
    [[nodiscard]] Check check() const;
    /// The latest version of `id` as the last check read it.
    [[nodiscard]] std::optional<Build> latest(std::string_view id) const;
    /// Whether the last check found a newer version than the one openSU downloaded.
    [[nodiscard]] bool updateAvailable(std::string_view id) const;

    /// Removes the AppImage or core openSU downloaded for `id`; an emulator installed another way
    /// is never touched. Empty on success, else why not.
    [[nodiscard]] std::string remove(std::string_view id);

    /// The version openSU downloaded for `id`.
    [[nodiscard]] std::optional<Build> downloaded(std::string_view id) const;

    /// Hands what happened on the threads to the loop through the hooks. Main loop only.
    void service();

  private:
    struct Ended {
        std::string id;
        std::string text;
        Finished::Outcome outcome{Finished::Outcome::Installed};
    };

    void runDownload(const std::stop_token& stop, const library::roms::EmulatorEntry& entry);
    /// Installs the latest release of `entry`; nothing, with `error`, when it is not installed.
    [[nodiscard]] std::optional<Build> installRelease(const std::stop_token& stop,
                                                      const library::roms::EmulatorEntry& entry,
                                                      const net::WebClient::Progress& progress,
                                                      std::string& error);
    /// Installs the latest build of `entry`'s core.
    [[nodiscard]] std::optional<Build> installCore(const std::stop_token& stop,
                                                   const library::roms::EmulatorEntry& entry,
                                                   const net::WebClient::Progress& progress,
                                                   std::string& error);
    /// Reads the latest builds; releases of standalone emulators only when `standalone`.
    void runCheck(const std::stop_token& stop, bool standalone);
    /// Reads the buildbot's listing once for every built-in emulator.
    void checkCores(std::vector<std::string>& failures);
    /// Fetches the info of each core openSU downloaded without one, the archive once.
    void fetchMissingInfo(std::vector<std::string>& failures);
    void remember(const std::string& id, Build build);

    library::roms::EmulatorSetup setup_;
    library::roms::DownloadedEmulators store_;
    library::roms::DownloadedCores cores_;
    Hooks hooks_;
    net::WebClient web_;
    library::roms::EmulatorRelease release_{web_};
    library::roms::CoreDownload coreDownload_{web_, setup_.coreSource};

    mutable std::mutex mutex_;
    std::optional<Progress> progress_;
    std::map<std::string, Build, std::less<>> latest_;
    Check check_;
    std::vector<Ended> ended_;
    std::optional<Finished> finished_;
    /// The loop's last look, so progress refreshes once per percent.
    std::optional<int> shownPercent_;
    bool checkChanged_{false};
    /// Last, so they stop before what they use goes.
    std::jthread checker_;
    std::jthread downloader_;
};

} // namespace opensu::app

// emulator_downloads — downloading, updating and removing what openSU installs for emulators, off
// the main loop: official AppImages, built-in emulators' libretro cores and libretro's shader pack
// (`shaderPackId`) and the visual presets' art (`overlayArtId`), one download at a time with its
// progress and cancel, the update check (at start and daily while openSU downloaded something, and
// when asked), and the news the loop hands to the screens (`service`). Nothing updates unless the
// player asks.
#pragma once

#include <chrono>
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
    using Clock = std::chrono::steady_clock;

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
        /// What tells two versions apart: a release asset's id, a buildbot core's CRC-32, a
        /// released core's URL (`keyOf`).
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
    void setStandalone(bool standalone);

    /// Downloads the latest release, core build, shader pack (`shaderPackId`) or the missing
    /// overlay art (`overlayArtId`) of `id` and switches to it. Refused, with the reason, while
    /// another download runs or for an emulator openSU cannot download.
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
    /// A newer version than the one openSU downloaded.
    struct Update {
        std::string id;
        Build have;
        Build newest;
    };
    /// Whether the last check found a newer version than the one openSU downloaded. Main loop only.
    [[nodiscard]] bool updateAvailable(std::string_view id) const;
    /// Every update the last check found for what counts now, in catalog order after the shader
    /// pack; kept current as checks, downloads and removals end. Main loop only.
    [[nodiscard]] const std::vector<Update>& updates() const noexcept {
        return updates_;
    }

    /// Removes the AppImage or core openSU downloaded for `id`; an emulator installed another way
    /// is never touched. Empty on success, else why not.
    [[nodiscard]] std::string remove(std::string_view id);

    /// The version openSU downloaded for `id`.
    [[nodiscard]] std::optional<Build> downloaded(std::string_view id) const;

    /// `id`'s name as the player reads it: the emulator's, the shader pack's or the art's.
    [[nodiscard]] std::string nameOf(std::string_view id) const;
    /// The shader pack on disk.
    [[nodiscard]] const library::roms::ShaderPack& shaderPack() const noexcept {
        return setup_.shaders;
    }
    /// The visual presets and their art on disk.
    [[nodiscard]] const library::roms::VisualPresets& presets() const noexcept {
        return setup_.presets;
    }

    /// Hands what happened on the threads to the loop through the hooks and starts the update
    /// check when it is due. Main loop only.
    void service();

  private:
    struct Ended {
        std::string id;
        std::string text;
        Finished::Outcome outcome{Finished::Outcome::Installed};
    };

    /// One download's work: the version it installed.
    using Installer =
        std::function<net::Result<Build>(const std::stop_token&, const net::WebClient::Progress&)>;

    /// Starts `install` on the download thread as the download of `id`; refused while one runs.
    [[nodiscard]] std::string launch(std::string_view id, Installer install);
    void runDownload(const std::stop_token& stop, const std::string& id, const Installer& install);
    /// Installs the latest release of `entry`.
    [[nodiscard]] net::Result<Build> installRelease(const std::stop_token& stop,
                                                    const library::roms::EmulatorEntry& entry,
                                                    const net::WebClient::Progress& progress);
    /// Installs the latest build of `entry`'s core, from its release when the catalog names one.
    [[nodiscard]] net::Result<Build> installCore(const std::stop_token& stop,
                                                 const library::roms::EmulatorEntry& entry,
                                                 const net::WebClient::Progress& progress);
    /// Installs the shader pack the buildbot serves now.
    [[nodiscard]] net::Result<Build> installPack(const std::stop_token& stop,
                                                 const net::WebClient::Progress& progress);
    /// Downloads the overlay art that is missing.
    [[nodiscard]] net::Result<Build> installArt(const std::stop_token& stop,
                                                const net::WebClient::Progress& progress);
    /// Reads the latest builds; releases of standalone emulators only when `standalone`.
    void runCheck(const std::stop_token& stop, bool standalone);
    /// Reads the buildbot's listing once for every built-in emulator without a release.
    void checkCores(const std::stop_token& stop, std::vector<std::string>& failures);
    /// Fetches the info of each core openSU downloaded without one, the archive once.
    void fetchMissingInfo(const std::stop_token& stop, std::vector<std::string>& failures);
    void remember(const std::string& id, Build build);
    /// The ids an update can be offered for: the shader pack and the catalog's that count now.
    [[nodiscard]] std::vector<std::string> updatable() const;
    /// Whether openSU downloaded any of them.
    [[nodiscard]] bool anyDownloaded() const;
    /// Reads `updates_` again.
    void listUpdates();
    /// Starts the update check once a day, the first at start, while something is downloaded.
    void checkWhenDue(Clock::time_point now);

    library::roms::EmulatorSetup setup_;
    library::roms::DownloadedEmulators store_;
    library::roms::DownloadedCores cores_;
    Hooks hooks_;
    net::WebClient web_;
    library::roms::EmulatorRelease release_{web_};
    library::roms::CoreDownload coreDownload_{web_, setup_.coreSource};
    library::roms::ShaderPackDownload packDownload_{web_, setup_.shaderSource};

    mutable std::mutex mutex_;
    std::optional<Progress> progress_;
    std::map<std::string, Build, std::less<>> latest_;
    Check check_;
    std::vector<Ended> ended_;
    std::optional<Finished> finished_;
    /// The loop's last look, so progress refreshes once per percent.
    std::optional<int> shownPercent_;
    bool checkChanged_{false};
    /// When the next scheduled check is due; at the first service before any.
    std::optional<Clock::time_point> nextCheck_;
    std::vector<Update> updates_;
    /// Last, so they stop before what they use goes.
    std::jthread checker_;
    std::jthread downloader_;
};

} // namespace opensu::app

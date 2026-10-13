// emulator_downloads — downloading, updating and removing the emulators openSU installs, off the
// main loop: one download at a time with its progress and cancel, the update check, and the news
// the loop hands to the screens (`service`). Nothing updates unless the player asks.
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
    /// Searches for the emulators now.
    [[nodiscard]] library::roms::Emulators discover() const {
        return setup_.discover();
    }

    /// Downloads the latest release of `id` and switches to it. Refused, with the reason, while
    /// another download runs or for an emulator without official AppImages.
    [[nodiscard]] std::string start(std::string_view id);
    /// Stops the running download of `id`; the version in use stays.
    void cancel(std::string_view id);
    /// The running download, or nothing.
    [[nodiscard]] std::optional<Progress> progress() const;

    /// Reads the latest release of every downloadable emulator, in the background.
    void checkUpdates();
    [[nodiscard]] Check check() const;
    /// The latest release of `id` as the last check read it.
    [[nodiscard]] std::optional<library::roms::ReleaseAsset> latest(std::string_view id) const;
    /// Whether the last check found a newer release than the one openSU downloaded.
    [[nodiscard]] bool updateAvailable(std::string_view id) const;

    /// Removes the AppImage openSU downloaded for `id`; an emulator installed another way is
    /// never touched. Empty on success, else why not.
    [[nodiscard]] std::string remove(std::string_view id);

    /// The version openSU downloaded for `id`.
    [[nodiscard]] std::optional<library::roms::DownloadedEmulator>
    downloaded(std::string_view id) const;

    /// Hands what happened on the threads to the loop through the hooks. Main loop only.
    void service();

  private:
    struct Ended {
        std::string text;
        bool error{false};
        bool installed{false};
    };

    void runDownload(const std::stop_token& stop, const library::roms::EmulatorEntry& entry,
                     const library::roms::ReleaseSource& source);
    void runCheck(const std::stop_token& stop);

    library::roms::EmulatorSetup setup_;
    library::roms::DownloadedEmulators store_;
    Hooks hooks_;
    net::WebClient web_;
    library::roms::EmulatorRelease release_{web_};

    mutable std::mutex mutex_;
    std::optional<Progress> progress_;
    std::map<std::string, library::roms::ReleaseAsset, std::less<>> latest_;
    Check check_;
    std::vector<Ended> ended_;
    /// The loop's last look, so progress refreshes once per percent.
    std::optional<int> shownPercent_;
    bool checkChanged_{false};
    /// Last, so they stop before what they use goes.
    std::jthread checker_;
    std::jthread downloader_;
};

} // namespace opensu::app

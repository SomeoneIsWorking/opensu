#include "emulator_downloads.hpp"

#include "lucent/log.h"

namespace opensu::app {
namespace {

using library::roms::EmulatorEntry;

constexpr std::uint64_t percentScale = 100;

} // namespace

std::optional<int> EmulatorDownloads::Progress::percent() const noexcept {
    if (total == 0) {
        return std::nullopt;
    }
    return static_cast<int>(std::min(received, total) * percentScale / total);
}

EmulatorDownloads::EmulatorDownloads(library::roms::EmulatorSetup setup, Hooks hooks)
    : setup_{std::move(setup)}, store_{setup_.search.downloads}, hooks_{std::move(hooks)} {
}

EmulatorDownloads::~EmulatorDownloads() {
    downloader_.request_stop();
    checker_.request_stop();
}

std::string EmulatorDownloads::start(std::string_view id) {
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr || !entry->release) {
        return "openSU cannot download " + std::string{entry == nullptr ? id : entry->name};
    }
    const library::roms::ReleaseSource* source = &*entry->release;
    {
        const std::lock_guard lock{mutex_};
        if (progress_) {
            const EmulatorEntry* running = setup_.catalog.find(progress_->id);
            return (running != nullptr ? running->name : progress_->id) +
                   " is downloading; one emulator downloads at a time";
        }
        progress_ = Progress{std::string{id}, 0, 0};
        shownPercent_.reset();
    }
    downloader_ = std::jthread{[this, entry, source](const std::stop_token& stop) {
        runDownload(stop, *entry, *source);
    }};
    return {};
}

void EmulatorDownloads::runDownload(const std::stop_token& stop, const EmulatorEntry& entry,
                                    const library::roms::ReleaseSource& source) {
    std::string error;
    std::optional<library::roms::ReleaseAsset> asset = release_.latest(source, error);
    std::optional<library::roms::DownloadedEmulator> installed;
    if (asset && !stop.stop_requested()) {
        {
            const std::lock_guard lock{mutex_};
            latest_.insert_or_assign(entry.id, *asset);
        }
        installed = release_.install(
            entry, *asset, store_,
            [this, &stop](std::uint64_t received, std::uint64_t total) {
                const std::lock_guard lock{mutex_};
                if (progress_) {
                    progress_->received = received;
                    progress_->total = total;
                }
                return !stop.stop_requested();
            },
            error);
    } else if (asset) {
        error = "cancelled";
    }
    Ended ended;
    if (installed) {
        ended = Ended{entry.name + " " + installed->version + " is installed", false, true};
    } else if (error == "cancelled") {
        ended = Ended{entry.name + " download cancelled", false, false};
    } else {
        ended = Ended{entry.name + " was not installed: " + error, true, false};
        lucent::warn("emulators", "{}", ended.text);
    }
    const std::lock_guard lock{mutex_};
    progress_.reset();
    ended_.push_back(std::move(ended));
}

void EmulatorDownloads::cancel(std::string_view id) {
    const std::lock_guard lock{mutex_};
    if (progress_ && progress_->id == id) {
        downloader_.request_stop();
    }
}

std::optional<EmulatorDownloads::Progress> EmulatorDownloads::progress() const {
    const std::lock_guard lock{mutex_};
    return progress_;
}

void EmulatorDownloads::checkUpdates() {
    {
        const std::lock_guard lock{mutex_};
        if (check_.running) {
            return;
        }
        check_ = Check{.running = true};
        checkChanged_ = true;
    }
    checker_ = std::jthread{[this](const std::stop_token& stop) {
        runCheck(stop);
    }};
}

void EmulatorDownloads::runCheck(const std::stop_token& stop) {
    std::vector<std::string> failures;
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (stop.stop_requested()) {
            return;
        }
        if (!entry.release) {
            continue;
        }
        std::string error;
        if (std::optional<library::roms::ReleaseAsset> asset =
                release_.latest(*entry.release, error)) {
            const std::lock_guard lock{mutex_};
            latest_.insert_or_assign(entry.id, std::move(*asset));
        } else {
            lucent::warn("emulators", "{}: {}", entry.name, error);
            failures.push_back(entry.name + ": " + error);
        }
    }
    const std::lock_guard lock{mutex_};
    check_ = Check{.running = false, .done = true, .failures = std::move(failures)};
    checkChanged_ = true;
}

EmulatorDownloads::Check EmulatorDownloads::check() const {
    const std::lock_guard lock{mutex_};
    return check_;
}

std::optional<library::roms::ReleaseAsset> EmulatorDownloads::latest(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const auto found = latest_.find(id);
    return found == latest_.end() ? std::nullopt : std::optional{found->second};
}

std::optional<library::roms::DownloadedEmulator>
EmulatorDownloads::downloaded(std::string_view id) const {
    return store_.find(id);
}

bool EmulatorDownloads::updateAvailable(std::string_view id) const {
    const std::optional<library::roms::ReleaseAsset> newest = latest(id);
    const std::optional<library::roms::DownloadedEmulator> have = downloaded(id);
    return newest && have && newest->assetId != have->assetId;
}

std::string EmulatorDownloads::remove(std::string_view id) {
    {
        const std::lock_guard lock{mutex_};
        if (progress_ && progress_->id == id) {
            return "it is downloading; cancel the download first";
        }
    }
    if (!store_.find(id)) {
        return "openSU did not download it";
    }
    std::string error;
    if (!store_.remove(id, error)) {
        return error;
    }
    if (hooks_.installedChanged) {
        hooks_.installedChanged();
    }
    return {};
}

void EmulatorDownloads::service() {
    std::vector<Ended> ended;
    bool refresh = false;
    {
        const std::lock_guard lock{mutex_};
        ended = std::exchange(ended_, {});
        const std::optional<int> percent =
            progress_ ? progress_->percent().value_or(0) : std::optional<int>{};
        refresh = std::exchange(checkChanged_, false) || percent != shownPercent_;
        shownPercent_ = percent;
    }
    for (const Ended& one : ended) {
        if (downloader_.joinable()) {
            downloader_.join();
        }
        hooks_.say(one.text, one.error);
        if (one.installed) {
            hooks_.installedChanged();
        }
    }
    if (refresh || !ended.empty()) {
        hooks_.refresh();
    }
}

} // namespace opensu::app

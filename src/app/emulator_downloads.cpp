#include "emulator_downloads.hpp"

#include <algorithm>

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
    : setup_{std::move(setup)}, store_{setup_.search.downloads},
      cores_{setup_.search.cores.downloads}, hooks_{std::move(hooks)} {
}

EmulatorDownloads::~EmulatorDownloads() {
    downloader_.request_stop();
    checker_.request_stop();
}

std::string EmulatorDownloads::start(std::string_view id) {
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr || !entry->downloadable()) {
        return "openSU cannot download " + std::string{entry == nullptr ? id : entry->name};
    }
    if (entry->builtIn() && cores_.root().empty()) {
        return "openSU has no folder to keep " + entry->name + "'s core in";
    }
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
    downloader_ = std::jthread{[this, entry](const std::stop_token& stop) {
        runDownload(stop, *entry);
    }};
    return {};
}

void EmulatorDownloads::remember(const std::string& id, Build build) {
    const std::lock_guard lock{mutex_};
    latest_.insert_or_assign(id, std::move(build));
}

std::optional<EmulatorDownloads::Build>
EmulatorDownloads::installRelease(const std::stop_token& stop, const EmulatorEntry& entry,
                                  const net::WebClient::Progress& progress, std::string& error) {
    if (!entry.release) {
        error = entry.name + " publishes no AppImage";
        return std::nullopt;
    }
    const std::optional<library::roms::ReleaseAsset> asset = release_.latest(*entry.release, error);
    if (!asset) {
        return std::nullopt;
    }
    remember(entry.id, Build{asset->version, asset->assetId});
    if (stop.stop_requested()) {
        error = "cancelled";
        return std::nullopt;
    }
    const std::optional<library::roms::DownloadedEmulator> installed =
        release_.install(entry, *asset, store_, progress, error);
    if (!installed) {
        return std::nullopt;
    }
    return Build{installed->version, installed->assetId};
}

std::optional<EmulatorDownloads::Build>
EmulatorDownloads::installCore(const std::stop_token& stop, const EmulatorEntry& entry,
                               const net::WebClient::Progress& progress, std::string& error) {
    const std::optional<library::roms::CoreBuild> build = coreDownload_.latest(entry.core, error);
    if (!build) {
        return std::nullopt;
    }
    remember(entry.id, Build{build->date, std::to_string(build->crc32)});
    if (stop.stop_requested()) {
        error = "cancelled";
        return std::nullopt;
    }
    const std::optional<library::roms::DownloadedCore> installed =
        coreDownload_.install(*build, cores_, progress, error);
    if (!installed) {
        return std::nullopt;
    }
    return Build{installed->date, std::to_string(installed->crc32)};
}

void EmulatorDownloads::runDownload(const std::stop_token& stop, const EmulatorEntry& entry) {
    const net::WebClient::Progress progress = [this, &stop](std::uint64_t received,
                                                            std::uint64_t total) {
        const std::lock_guard lock{mutex_};
        if (progress_) {
            progress_->received = received;
            progress_->total = total;
        }
        return !stop.stop_requested();
    };
    std::string error;
    const std::optional<Build> installed = entry.builtIn()
                                               ? installCore(stop, entry, progress, error)
                                               : installRelease(stop, entry, progress, error);
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
    checkCores(failures);
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (stop.stop_requested()) {
            return;
        }
        if (!entry.release) {
            continue;
        }
        std::string error;
        if (const std::optional<library::roms::ReleaseAsset> asset =
                release_.latest(*entry.release, error)) {
            remember(entry.id, Build{asset->version, asset->assetId});
        } else {
            lucent::warn("emulators", "{}: {}", entry.name, error);
            failures.push_back(entry.name + ": " + error);
        }
    }
    const std::lock_guard lock{mutex_};
    check_ = Check{.running = false, .done = true, .failures = std::move(failures)};
    checkChanged_ = true;
}

void EmulatorDownloads::checkCores(std::vector<std::string>& failures) {
    if (std::ranges::none_of(setup_.catalog.entries(), &EmulatorEntry::builtIn)) {
        return;
    }
    std::string error;
    const std::optional<std::string> listing = coreDownload_.index(error);
    if (!listing) {
        lucent::warn("emulators", "libretro buildbot: {}", error);
        failures.push_back("libretro buildbot: " + error);
        return;
    }
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (!entry.builtIn()) {
            continue;
        }
        if (const std::optional<library::roms::CoreBuild> build =
                library::roms::coreBuildIn(*listing, entry.core, coreDownload_.base(), error)) {
            remember(entry.id, Build{build->date, std::to_string(build->crc32)});
        } else {
            lucent::warn("emulators", "{}: {}", entry.name, error);
            failures.push_back(entry.name + ": " + error);
        }
    }
}

EmulatorDownloads::Check EmulatorDownloads::check() const {
    const std::lock_guard lock{mutex_};
    return check_;
}

std::optional<EmulatorDownloads::Build> EmulatorDownloads::latest(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const auto found = latest_.find(id);
    return found == latest_.end() ? std::nullopt : std::optional{found->second};
}

std::optional<EmulatorDownloads::Build> EmulatorDownloads::downloaded(std::string_view id) const {
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr) {
        return std::nullopt;
    }
    if (entry->builtIn()) {
        const std::optional<library::roms::DownloadedCore> core = cores_.find(entry->core);
        return core ? std::optional{Build{core->date, std::to_string(core->crc32)}} : std::nullopt;
    }
    const std::optional<library::roms::DownloadedEmulator> have = store_.find(id);
    return have ? std::optional{Build{have->version, have->assetId}} : std::nullopt;
}

bool EmulatorDownloads::updateAvailable(std::string_view id) const {
    const std::optional<Build> newest = latest(id);
    const std::optional<Build> have = downloaded(id);
    return newest && have && newest->key != have->key;
}

std::string EmulatorDownloads::remove(std::string_view id) {
    {
        const std::lock_guard lock{mutex_};
        if (progress_ && progress_->id == id) {
            return "it is downloading; cancel the download first";
        }
    }
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr || !downloaded(id)) {
        return "openSU did not download it";
    }
    std::string error;
    if (!(entry->builtIn() ? cores_.remove(entry->core, error) : store_.remove(id, error))) {
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
        // A download runs until its end is reported here, on the caller's thread.
        if (!ended.empty()) {
            progress_.reset();
        }
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

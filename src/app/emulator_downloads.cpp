#include "emulator_downloads.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "library/core_bios.hpp"
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
    using library::roms::shaderPackId;
    if (id == shaderPackId) {
        if (setup_.shaders.root().parent_path().empty()) {
            return "openSU has no folder to keep the shader presets in";
        }
        return launch(id, [this](const std::stop_token& stop, const auto& progress, auto& error) {
            return installPack(stop, progress, error);
        });
    }
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr || !entry->downloadable()) {
        return "openSU cannot download " + std::string{entry == nullptr ? id : entry->name};
    }
    if (!setup_.counts(*entry)) {
        return "standalone emulators are off in Settings > Integrations";
    }
    if (entry->builtIn() && cores_.root().empty()) {
        return "openSU has no folder to keep " + entry->name + "'s core in";
    }
    return launch(id,
                  [this, entry](const std::stop_token& stop, const auto& progress, auto& error) {
                      return entry->builtIn() ? installCore(stop, *entry, progress, error)
                                              : installRelease(stop, *entry, progress, error);
                  });
}

std::string EmulatorDownloads::launch(std::string_view id, Installer install) {
    {
        const std::lock_guard lock{mutex_};
        if (progress_) {
            return nameOf(progress_->id) + " is downloading; one download runs at a time";
        }
        progress_ = Progress{std::string{id}, 0, 0};
        shownPercent_.reset();
    }
    downloader_ = std::jthread{
        [this, id = std::string{id}, install = std::move(install)](const std::stop_token& stop) {
            runDownload(stop, id, install);
        }};
    return {};
}

std::string EmulatorDownloads::nameOf(std::string_view id) const {
    if (id == library::roms::shaderPackId) {
        return std::string{library::roms::shaderPackName};
    }
    const EmulatorEntry* entry = setup_.catalog.find(id);
    return entry != nullptr ? entry->name : std::string{id};
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

std::optional<EmulatorDownloads::Build>
EmulatorDownloads::installPack(const std::stop_token& stop,
                               const net::WebClient::Progress& progress, std::string& error) {
    if (stop.stop_requested()) {
        error = "cancelled";
        return std::nullopt;
    }
    const std::optional<library::roms::ShaderPackBuild> installed =
        packDownload_.install(setup_.shaders, progress, error);
    if (!installed) {
        return std::nullopt;
    }
    Build build{installed->version(), installed->key()};
    remember(std::string{library::roms::shaderPackId}, build);
    return build;
}

void EmulatorDownloads::runDownload(const std::stop_token& stop, const std::string& id,
                                    const Installer& install) {
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
    const std::optional<Build> installed = install(stop, progress, error);
    const std::string name = nameOf(id);
    using Outcome = Finished::Outcome;
    Ended ended;
    if (installed) {
        ended = Ended{id, name + " " + installed->version + " is installed", Outcome::Installed};
    } else if (error == "cancelled") {
        ended = Ended{id, name + " download cancelled", Outcome::Cancelled};
    } else {
        ended = Ended{id, name + " was not installed: " + error, Outcome::Failed};
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

std::optional<EmulatorDownloads::Finished> EmulatorDownloads::lastFinished() const {
    const std::lock_guard lock{mutex_};
    return finished_;
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
    checker_ = std::jthread{[this, standalone = setup_.standalone](const std::stop_token& stop) {
        runCheck(stop, standalone);
    }};
}

void EmulatorDownloads::runCheck(const std::stop_token& stop, bool standalone) {
    std::vector<std::string> failures;
    checkCores(failures);
    if (!setup_.shaders.root().parent_path().empty()) {
        std::string error;
        if (const std::optional<library::roms::ShaderPackBuild> pack =
                packDownload_.latest(error)) {
            remember(std::string{library::roms::shaderPackId}, Build{pack->version(), pack->key()});
        } else {
            lucent::warn("emulators", "{}", error);
            failures.push_back(std::string{library::roms::shaderPackName} + ": " + error);
        }
    }
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (stop.stop_requested()) {
            return;
        }
        if (!entry.release || (!standalone && !entry.builtIn())) {
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
    fetchMissingInfo(failures);
}

void EmulatorDownloads::fetchMissingInfo(std::vector<std::string>& failures) {
    std::optional<library::roms::CoreInfoArchive> archive;
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        std::error_code ec;
        if (!entry.builtIn() || !cores_.find(entry.core) ||
            std::filesystem::is_regular_file(cores_.info(entry.core), ec)) {
            continue;
        }
        std::string error;
        if (!archive) {
            std::optional<std::string> bytes = coreDownload_.infoArchive(error);
            archive = bytes ? library::roms::CoreInfoArchive::open(std::move(*bytes), error)
                            : std::nullopt;
        }
        const std::optional<std::string> info =
            archive ? archive->info(entry.core, error) : std::nullopt;
        if (!info || !cores_.installInfo(entry.core, *info, error)) {
            lucent::warn("emulators", "{}: {}", entry.name, error);
            failures.push_back(entry.name + ": " + error);
            if (!archive) {
                return;
            }
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
    if (id == library::roms::shaderPackId) {
        const std::optional<library::roms::ShaderPackBuild> pack = setup_.shaders.installed();
        return pack ? std::optional{Build{pack->version(), pack->key()}} : std::nullopt;
    }
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
    if (!downloaded(id)) {
        return "openSU did not download it";
    }
    std::string error;
    const EmulatorEntry* entry = setup_.catalog.find(id);
    bool removed = false;
    if (id == library::roms::shaderPackId) {
        removed = setup_.shaders.remove(error);
    } else if (entry != nullptr) {
        removed = entry->builtIn() ? cores_.remove(entry->core, error) : store_.remove(id, error);
    }
    if (!removed) {
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
            const Ended& last = ended.back();
            finished_ = Finished{.serial = finished_ ? finished_->serial + 1 : 1,
                                 .id = last.id,
                                 .outcome = last.outcome,
                                 .text = last.text};
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
        hooks_.say(one.text, one.outcome == Finished::Outcome::Failed);
        if (one.outcome == Finished::Outcome::Installed) {
            hooks_.installedChanged();
        }
    }
    if (refresh || !ended.empty()) {
        hooks_.refresh();
    }
}

} // namespace opensu::app

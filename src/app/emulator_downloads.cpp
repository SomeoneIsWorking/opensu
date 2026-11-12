#include "emulator_downloads.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>

#include "library/core_bios.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

using library::roms::EmulatorEntry;

constexpr std::uint64_t percentScale = 100;
constexpr std::chrono::hours checkEvery{24};

/// A downloaded core's key: a released core's record keeps no asset id, but its URL names its tag.
std::string keyOf(const EmulatorEntry& entry, const library::roms::DownloadedCore& core) {
    return entry.release ? core.url : std::to_string(core.crc32);
}

/// The key of a release's `asset` for `entry`, as `keyOf` reads it once it is installed.
std::string keyOf(const EmulatorEntry& entry, const library::roms::ReleaseAsset& asset) {
    return entry.builtIn() ? asset.url : asset.assetId;
}

} // namespace

std::optional<int> EmulatorDownloads::Progress::percent() const noexcept {
    if (total == 0) {
        return std::nullopt;
    }
    return static_cast<int>(std::min(received, total) * percentScale / total);
}

EmulatorDownloads::EmulatorDownloads(library::roms::EmulatorSetup setup, Hooks hooks)
    : setup_{std::move(setup)}, store_{setup_.search.downloads},
      cores_{setup_.search.cores.downloads},
      systemFiles_{setup_.search.system, setup_.search.cores.downloads}, hooks_{std::move(hooks)} {
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
        return launch(id, [this](const std::stop_token& stop, const auto& progress) {
            return installPack(stop, progress);
        });
    }
    if (id == library::roms::overlayArtId) {
        if (setup_.presets.folder().empty()) {
            return "openSU has no folder to keep the overlay art in";
        }
        return launch(id, [this](const std::stop_token& stop, const auto& progress) {
            return installArt(stop, progress);
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
    return launch(id, [this, entry](const std::stop_token& stop, const auto& progress) {
        return entry->builtIn() ? installCore(stop, *entry, progress)
                                : installRelease(stop, *entry, progress);
    });
}

std::string EmulatorDownloads::launch(std::string_view id, Installer install,
                                      std::string installed) {
    {
        const std::lock_guard lock{mutex_};
        if (progress_) {
            return nameOf(progress_->id) + " is downloading; one download runs at a time";
        }
        progress_ = Progress{std::string{id}, 0, 0};
        shownPercent_.reset();
    }
    downloader_ = std::jthread{[this, id = std::string{id}, install = std::move(install),
                                installed = std::move(installed)](const std::stop_token& stop) {
        runDownload(stop, id, install, installed);
    }};
    return {};
}

std::string EmulatorDownloads::nameOf(std::string_view id) const {
    if (id == library::roms::shaderPackId) {
        return std::string{library::roms::shaderPackName};
    }
    if (id == library::roms::overlayArtId) {
        return std::string{library::roms::overlayArtName};
    }
    const EmulatorEntry* entry = setup_.catalog.find(id);
    return entry != nullptr ? entry->name : std::string{id};
}

void EmulatorDownloads::remember(const std::string& id, Build build) {
    const std::lock_guard lock{mutex_};
    latest_.insert_or_assign(id, std::move(build));
}

net::Result<EmulatorDownloads::Build>
EmulatorDownloads::installRelease(const std::stop_token& stop, const EmulatorEntry& entry,
                                  const net::WebClient::Progress& progress) {
    if (!entry.release) {
        return net::Failure{entry.name + " publishes no AppImage"};
    }
    const net::Result<library::roms::ReleaseAsset> asset = release_.latest(stop, *entry.release);
    if (!asset) {
        return asset.as<Build>();
    }
    remember(entry.id, Build{asset->version, keyOf(entry, *asset)});
    const net::Result<library::roms::DownloadedEmulator> installed =
        release_.install(stop, entry, *asset, store_, progress);
    if (!installed) {
        return installed.as<Build>();
    }
    return Build{installed->version, installed->assetId};
}

net::Result<EmulatorDownloads::Build>
EmulatorDownloads::installCore(const std::stop_token& stop, const EmulatorEntry& entry,
                               const net::WebClient::Progress& progress) {
    if (entry.release) {
        const net::Result<library::roms::ReleaseAsset> asset =
            release_.latest(stop, *entry.release);
        if (!asset) {
            return asset.as<Build>();
        }
        remember(entry.id, Build{asset->version, keyOf(entry, *asset)});
        const net::Result<library::roms::DownloadedCore> installed = coreDownload_.installRelease(
            stop, entry.core, *asset, entry.release->library, release_, cores_, progress);
        if (!installed) {
            return installed.as<Build>();
        }
        if (const net::Result<std::size_t> files = installSystemFiles(stop, entry, progress);
            !files) {
            return files.as<Build>("its system files were not installed: ");
        }
        return Build{installed->date, keyOf(entry, *installed)};
    }
    const net::Result<library::roms::CoreBuild> build = coreDownload_.latest(stop, entry.core);
    if (!build) {
        return build.as<Build>();
    }
    remember(entry.id, Build{build->date, std::to_string(build->crc32)});
    const net::Result<library::roms::DownloadedCore> installed =
        coreDownload_.install(stop, *build, cores_, progress);
    if (!installed) {
        return installed.as<Build>();
    }
    if (const net::Result<std::size_t> files = installSystemFiles(stop, entry, progress); !files) {
        return files.as<Build>("its system files were not installed: ");
    }
    return Build{installed->date, keyOf(entry, *installed)};
}

net::Result<std::size_t>
EmulatorDownloads::installSystemFiles(const std::stop_token& stop, const EmulatorEntry& entry,
                                      const net::WebClient::Progress& progress) {
    if (!entry.systemFiles || setup_.search.system.empty() ||
        systemFiles_.current(entry.core, *entry.systemFiles)) {
        return std::size_t{0};
    }
    return systemFiles_.install(stop, web_, entry.core, *entry.systemFiles, progress);
}

net::Result<EmulatorDownloads::Build>
EmulatorDownloads::installPack(const std::stop_token& stop,
                               const net::WebClient::Progress& progress) {
    const net::Result<library::roms::ShaderPackBuild> installed =
        packDownload_.install(stop, setup_.shaders, progress);
    if (!installed) {
        return installed.as<Build>();
    }
    Build build{installed->version(), installed->key()};
    remember(std::string{library::roms::shaderPackId}, build);
    return build;
}

net::Result<EmulatorDownloads::Build>
EmulatorDownloads::installArt(const std::stop_token& stop,
                              const net::WebClient::Progress& progress) {
    const net::Result<std::size_t> installed = setup_.presets.install(stop, release_, progress);
    if (!installed) {
        return installed.as<Build>();
    }
    std::optional<Build> build = downloaded(library::roms::overlayArtId);
    if (!build) {
        return net::Failure{"the overlay art is not found where it was downloaded"};
    }
    return std::move(*build);
}

void EmulatorDownloads::runDownload(const std::stop_token& stop, const std::string& id,
                                    const Installer& install, const std::string& installed) {
    const net::WebClient::Progress progress = [this, &stop](std::uint64_t received,
                                                            std::uint64_t total) {
        const std::lock_guard lock{mutex_};
        if (progress_) {
            progress_->received = received;
            progress_->total = total;
        }
        return !stop.stop_requested();
    };
    const net::Result<Build> build = install(stop, progress);
    const std::string name = nameOf(id);
    using Outcome = Finished::Outcome;
    Ended ended;
    if (build) {
        ended =
            Ended{id, installed.empty() ? name + " " + build->version + " is installed" : installed,
                  Outcome::Installed};
    } else if (build.stopped()) {
        ended = Ended{id, name + " download cancelled", Outcome::Cancelled};
    } else {
        ended = Ended{id, name + " was not installed: " + build.error(), Outcome::Failed};
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
    checkCores(stop, failures);
    if (stop.stop_requested()) {
        return;
    }
    if (!setup_.shaders.root().parent_path().empty()) {
        if (const net::Result<library::roms::ShaderPackBuild> pack = packDownload_.latest(stop)) {
            remember(std::string{library::roms::shaderPackId}, Build{pack->version(), pack->key()});
        } else if (pack.stopped()) {
            return;
        } else {
            lucent::warn("emulators", "{}", pack.error());
            failures.push_back(std::string{library::roms::shaderPackName} + ": " + pack.error());
        }
    }
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (stop.stop_requested()) {
            return;
        }
        if (!entry.release || (!standalone && !entry.builtIn())) {
            continue;
        }
        if (const net::Result<library::roms::ReleaseAsset> asset =
                release_.latest(stop, *entry.release)) {
            remember(entry.id, Build{asset->version, keyOf(entry, *asset)});
        } else if (asset.stopped()) {
            return;
        } else {
            lucent::warn("emulators", "{}: {}", entry.name, asset.error());
            failures.push_back(entry.name + ": " + asset.error());
        }
    }
    const std::lock_guard lock{mutex_};
    check_ = Check{.running = false, .done = true, .failures = std::move(failures)};
    checkChanged_ = true;
}

void EmulatorDownloads::checkCores(const std::stop_token& stop,
                                   std::vector<std::string>& failures) {
    const auto fromBuildbot = [](const EmulatorEntry& entry) {
        return entry.builtIn() && !entry.release;
    };
    if (std::ranges::none_of(setup_.catalog.entries(), fromBuildbot)) {
        return;
    }
    const net::Result<std::string> listing = coreDownload_.index(stop);
    if (listing.stopped()) {
        return;
    }
    if (!listing) {
        lucent::warn("emulators", "libretro buildbot: {}", listing.error());
        failures.push_back("libretro buildbot: " + listing.error());
        return;
    }
    std::string error;
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (!fromBuildbot(entry)) {
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
    fetchMissingInfo(stop, failures);
    fetchMissingSystemFiles(stop, failures);
}

void EmulatorDownloads::fetchMissingSystemFiles(const std::stop_token& stop,
                                                std::vector<std::string>& failures) {
    const auto keepGoing = [](std::uint64_t, std::uint64_t) {
        return true;
    };
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (stop.stop_requested()) {
            return;
        }
        if (!entry.systemFiles || !setup_.search.cores.find(entry.core)) {
            continue;
        }
        const net::Result<std::size_t> files = installSystemFiles(stop, entry, keepGoing);
        if (files.stopped()) {
            return;
        }
        if (!files) {
            lucent::warn("emulators", "{}: its system files: {}", entry.name, files.error());
            failures.push_back(entry.name + ": its system files: " + files.error());
        }
    }
}

void EmulatorDownloads::fetchMissingInfo(const std::stop_token& stop,
                                         std::vector<std::string>& failures) {
    std::optional<library::roms::CoreInfoArchive> archive;
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        std::error_code ec;
        if (!entry.builtIn() || !cores_.find(entry.core) ||
            std::filesystem::is_regular_file(cores_.info(entry.core), ec)) {
            continue;
        }
        std::string error;
        if (!archive) {
            net::Result<std::string> bytes = coreDownload_.infoArchive(stop);
            if (bytes.stopped()) {
                return;
            }
            error = bytes.error();
            archive = bytes ? library::roms::CoreInfoArchive::open(*std::move(bytes), error)
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

void EmulatorDownloads::setStandalone(bool standalone) {
    setup_.standalone = standalone;
    listUpdates();
}

std::vector<std::string> EmulatorDownloads::updatable() const {
    std::vector<std::string> ids{std::string{library::roms::shaderPackId}};
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (entry.downloadable() && setup_.counts(entry)) {
            ids.push_back(entry.id);
        }
    }
    return ids;
}

bool EmulatorDownloads::anyDownloaded() const {
    return std::ranges::any_of(updatable(), [this](const std::string& id) {
        return downloaded(id).has_value();
    });
}

void EmulatorDownloads::listUpdates() {
    updates_.clear();
    for (std::string& id : updatable()) {
        std::optional<Build> newest = latest(id);
        if (!newest) {
            continue;
        }
        if (std::optional<Build> have = downloaded(id); have && have->key != newest->key) {
            updates_.push_back(Update{std::move(id), std::move(*have), std::move(*newest)});
        }
    }
}

void EmulatorDownloads::checkWhenDue(Clock::time_point now) {
    if (now < nextCheck_.value_or(now)) {
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        if (check_.running || progress_) {
            return;
        }
    }
    nextCheck_ = now + checkEvery;
    if (anyDownloaded()) {
        checkUpdates();
    }
}

bool EmulatorDownloads::moveToReleases() {
    if (progress() || (hooks_.gameRunning && hooks_.gameRunning())) {
        return false;
    }
    for (const EmulatorEntry& entry : setup_.catalog.entries()) {
        if (!entry.builtIn() || !entry.release || moved_.contains(entry.id) || !latest(entry.id)) {
            continue;
        }
        const std::optional<library::roms::DownloadedCore> core = cores_.find(entry.core);
        if (!core || !core->url.starts_with(coreDownload_.base()) || entry.fromRelease(core->url)) {
            continue;
        }
        moved_.insert(entry.id);
        const std::string told = entry.name + " moved to openSU's build" +
                                 (entry.links.empty() ? "" : ", which plays linked");
        lucent::info("emulators", "{}: replacing the buildbot core {} by its release's", entry.name,
                     core->date);
        // The next waits for this one to end.
        return !launch(
                    entry.id,
                    [this, moving = &entry](const std::stop_token& stop, const auto& progress) {
                        return installCore(stop, *moving, progress);
                    },
                    told)
                    .empty();
    }
    return true;
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
    if (id == library::roms::overlayArtId) {
        const library::roms::VisualPresets& presets = setup_.presets;
        const std::size_t have = presets.downloadedCount();
        if (have == 0) {
            return std::nullopt;
        }
        // The catalog pins each image, so the version is how many of them are here.
        const std::string version =
            std::to_string(have) + " of " + std::to_string(presets.art().size());
        return Build{version, version};
    }
    const EmulatorEntry* entry = setup_.catalog.find(id);
    if (entry == nullptr) {
        return std::nullopt;
    }
    if (entry->builtIn()) {
        const std::optional<library::roms::DownloadedCore> core = cores_.find(entry->core);
        return core ? std::optional{Build{core->date, keyOf(*entry, *core)}} : std::nullopt;
    }
    const std::optional<library::roms::DownloadedEmulator> have = store_.find(id);
    return have ? std::optional{Build{have->version, have->assetId}} : std::nullopt;
}

bool EmulatorDownloads::updateAvailable(std::string_view id) const {
    return std::ranges::find(updates_, id, &Update::id) != updates_.end();
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
    } else if (id == library::roms::overlayArtId) {
        removed = setup_.presets.remove(error);
    } else if (entry != nullptr) {
        removed = entry->builtIn()
                      ? cores_.remove(entry->core, error) && systemFiles_.remove(entry->core, error)
                      : store_.remove(id, error);
    }
    if (!removed) {
        return error;
    }
    listUpdates();
    if (hooks_.installedChanged) {
        hooks_.installedChanged();
    }
    return {};
}

void EmulatorDownloads::service() {
    std::vector<Ended> ended;
    bool refresh = false;
    bool checked = false;
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
        checked = std::exchange(checkChanged_, false);
        refresh = checked || percent != shownPercent_;
        shownPercent_ = percent;
    }
    const Clock::time_point now = Clock::now();
    if (checked || !ended.empty()) {
        listUpdates();
    }
    movePending_ = movePending_ || checked;
    if (movePending_ && ended.empty()) {
        movePending_ = !moveToReleases();
    }
    for (const Ended& one : ended) {
        if (downloader_.joinable()) {
            downloader_.join();
        }
        hooks_.say(one.text, one.outcome == Finished::Outcome::Failed);
        if (one.outcome == Finished::Outcome::Installed) {
            // What was just installed is the newest; the others wait for the day's check.
            nextCheck_ = nextCheck_.value_or(now + checkEvery);
            hooks_.installedChanged();
        }
    }
    checkWhenDue(now);
    if (refresh || !ended.empty()) {
        hooks_.refresh();
    }
}

} // namespace opensu::app

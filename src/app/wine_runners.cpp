#include "wine_runners.hpp"

#include <system_error>
#include <utility>

#include "launch/command.hpp"
#include "library/emulator_release.hpp"
#include "library/umu_launcher.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using Outcome = WineRunners::Finished::Outcome;

constexpr auto checkEvery = std::chrono::hours{24};
constexpr auto retryFailedAfter = std::chrono::hours{1};
constexpr const char* geId = "ge-proton";
constexpr const char* umuId = "umu-launcher";
constexpr int fullPercent = 100;
constexpr std::string_view tarSuffix = ".tar.gz";

std::uint64_t bytesUnder(const fs::path& dir) {
    std::uint64_t bytes = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator at{dir, ec}, end; !ec && at != end; at.increment(ec)) {
        std::error_code entry;
        if (at->is_regular_file(entry) && !at->is_symlink(entry)) {
            bytes += at->file_size(entry);
        }
    }
    return bytes;
}

fs::path trashDir(const fs::path& dataDir) {
    return dataDir / "wine" / ".trash";
}

/// Moves `dir` into the trash, where the sweeper removes it; false when it could not.
bool discard(const fs::path& dataDir, const fs::path& dir, const std::string& name) {
    std::error_code ec;
    fs::create_directories(trashDir(dataDir), ec);
    fs::path into = trashDir(dataDir) / name;
    for (int n = 1; fs::exists(into, ec); ++n) {
        into = trashDir(dataDir) / (name + "." + std::to_string(n));
    }
    fs::rename(dir, into, ec);
    if (ec) {
        lucent::warn("wine", "cannot remove {}: {}", dir.string(), ec.message());
        return false;
    }
    return true;
}

} // namespace

WineRunners::WineRunners(fs::path dataDir, std::string geApi, Hooks hooks)
    : dataDir_{std::move(dataDir)}, geApi_{std::move(geApi)}, hooks_{std::move(hooks)} {
    // Sizes are measured off the loop; a build is many thousand files.
    static_cast<void>(start([this](const std::stop_token&) {
        measure();
    }));
}

WineRunners::~WineRunners() {
    job_.request_stop();
}

bool WineRunners::start(Job job) {
    {
        const std::lock_guard lock{mutex_};
        if (running_) {
            return false;
        }
        running_ = true;
    }
    if (job_.joinable()) {
        job_.join();
    }
    job_ = std::jthread{[this, job = std::move(job)](const std::stop_token& stop) {
        job(stop);
        const std::lock_guard lock{mutex_};
        running_ = false;
    }};
    return true;
}

std::string WineRunners::checkNow() {
    if (!start([this](const std::stop_token& stop) {
            runCheck(stop);
        })) {
        return "A Wine runner download or test is running; try again when it ends";
    }
    return {};
}

void WineRunners::ensure() {
    static_cast<void>(checkNow());
}

void WineRunners::cancel() {
    if (busy()) {
        job_.request_stop();
    }
}

std::string WineRunners::test(std::string runner, std::vector<std::string> command) {
    if (!start([this, runner = std::move(runner),
                command = std::move(command)](const std::stop_token& stop) {
            runTest(stop, runner, command);
        })) {
        return "A Wine runner download or test is running; try again when it ends";
    }
    return {};
}

bool WineRunners::busy() const {
    const std::lock_guard lock{mutex_};
    return running_;
}

std::optional<WineRunners::Progress> WineRunners::progress() const {
    const std::lock_guard lock{mutex_};
    return progress_;
}

std::optional<WineRunners::Finished> WineRunners::lastFinished() const {
    const std::lock_guard lock{mutex_};
    return finished_;
}

WineRunners::Look WineRunners::look() const {
    const std::lock_guard lock{mutex_};
    return look_;
}

fs::path WineRunners::umuRun() const {
    const std::vector<library::umu::Install> installs =
        library::umu::installed(library::umu::launcherDir(dataDir_));
    return installs.empty() ? fs::path{} : installs.front().umuRun;
}

void WineRunners::runCheck(const std::stop_token& stop) {
    {
        const std::lock_guard lock{mutex_};
        look_.checking = true;
    }
    const bool ge = updateGeProton(stop);
    const bool umu = !stop.stop_requested() && updateUmu(stop);
    measure();
    const std::lock_guard lock{mutex_};
    look_.checking = false;
    checked_ = ge && umu;
    if (ge && umu) {
        look_.lastCheck =
            look_.builds.empty() ? "Checked" : "Up to date: " + look_.builds.front().name;
    } else if (finished_ && finished_->outcome == Outcome::Failed) {
        look_.lastCheck = "Check failed: " + finished_->text;
    } else {
        look_.lastCheck = stop.stop_requested() ? "Check cancelled" : "Check failed";
    }
}

bool WineRunners::updateGeProton(const std::stop_token& stop) {
    std::string error;
    const library::roms::EmulatorRelease releases{web_};
    const std::optional<library::roms::ReleaseAsset> tarball =
        releases.latest(library::ge_proton::tarballSource(geApi_), error);
    const std::optional<library::roms::ReleaseAsset> sums =
        tarball ? releases.latest(library::ge_proton::checksumSource(geApi_), error) : std::nullopt;
    if (!tarball || !sums) {
        ended(Outcome::Failed, geId, "GE-Proton", "GE-Proton's release cannot be read: " + error);
        return false;
    }
    const std::string tag = tarball->version;
    if (!library::ge_proton::versionOf(tag)) {
        ended(Outcome::Failed, geId, tag, "GE-Proton's release " + tag + " is not a build name");
        return false;
    }
    const fs::path runners = library::ge_proton::runnersDir(dataDir_);
    const std::vector<library::ge_proton::Build> builds = library::ge_proton::installed(runners);
    if (std::ranges::find(builds, tag, &library::ge_proton::Build::name) != builds.end()) {
        lucent::info("wine", "{} is the latest GE-Proton and installed", tag);
        return true;
    }
    const std::optional<std::string> listing = web_.get(sums->url, error);
    const std::optional<std::string> sha512 =
        listing ? library::ge_proton::sha512Listed(*listing, tarball->name) : std::nullopt;
    if (!sha512) {
        ended(Outcome::Failed, geId, tag,
              listing ? sums->name + " lists no SHA-512 for " + tarball->name
                      : "GE-Proton's checksum cannot be read: " + error);
        return false;
    }
    return install(stop, geId, tag,
                   library::RuntimeArchive{
                       .url = tarball->url,
                       .name = tarball->name,
                       .size = tarball->size,
                       .digest = library::RuntimeArchive::Digest::Sha512,
                       .hex = *sha512,
                       .folder = tarball->name.substr(0, tarball->name.size() - tarSuffix.size()),
                       .installAs = tag},
                   runners);
}

bool WineRunners::updateUmu(const std::stop_token& stop) {
    std::string error;
    const std::optional<library::roms::ReleaseAsset> zipapp =
        library::roms::EmulatorRelease{web_}.latest(library::umu::zipappSource(), error);
    const std::string name = zipapp ? "umu-launcher " + zipapp->version : "umu-launcher";
    if (!zipapp) {
        ended(Outcome::Failed, umuId, name, "umu-launcher's release cannot be read: " + error);
        return false;
    }
    if (zipapp->sha256.empty() || zipapp->version.find('/') != std::string::npos ||
        zipapp->version.starts_with('.')) {
        ended(Outcome::Failed, umuId, name,
              "umu-launcher " + zipapp->version + " publishes no SHA-256 to check it against");
        return false;
    }
    const fs::path into = library::umu::launcherDir(dataDir_) / zipapp->version;
    if (std::error_code ec; fs::is_regular_file(into / "umu" / "umu-run", ec)) {
        return true;
    }
    return install(stop, umuId, name,
                   library::RuntimeArchive{.url = zipapp->url,
                                           .name = zipapp->name,
                                           .size = zipapp->size,
                                           .digest = library::RuntimeArchive::Digest::Sha256,
                                           .hex = zipapp->sha256,
                                           .folder = "umu",
                                           .installAs = "umu"},
                   into);
}

bool WineRunners::install(const std::stop_token& stop, const std::string& id,
                          const std::string& name, const library::RuntimeArchive& archive,
                          const fs::path& into) {
    {
        const std::lock_guard lock{mutex_};
        progress_ =
            Progress{.id = id, .name = name, .stage = {}, .received = 0, .total = archive.size};
    }
    std::string error;
    const std::optional<fs::path> installed = library::installArchive(
        web_, archive, into, stop,
        [this, &stop](std::uint64_t received, std::uint64_t total) {
            const std::lock_guard lock{mutex_};
            if (progress_) {
                progress_->received = received;
                progress_->total = total != 0 ? total : progress_->total;
            }
            return !stop.stop_requested();
        },
        [this](library::ArchiveStage stage) {
            const std::lock_guard lock{mutex_};
            if (progress_) {
                progress_->stage = stage;
            }
        },
        error);
    {
        const std::lock_guard lock{mutex_};
        progress_.reset();
    }
    if (!installed) {
        lucent::warn("wine", "{} not installed: {}", name, error);
        ended(error == "cancelled" ? Outcome::Cancelled : Outcome::Failed, id, name,
              error == "cancelled" ? name + " download cancelled" : name + ": " + error);
        return false;
    }
    ended(Outcome::Installed, id, name, name + " installed");
    return true;
}

void WineRunners::runTest(const std::stop_token& stop, const std::string& runner,
                          const std::vector<std::string>& command) {
    {
        const std::lock_guard lock{mutex_};
        look_.testing = true;
    }
    std::string line = "env";
    for (const std::string& word : command) {
        line += " " + word;
    }
    lucent::info("wine", "testing {}: {}", runner, line);
    std::string answer;
    std::string last;
    const std::optional<int> status = launch::runStreaming(
        "env", command,
        [&answer, &last](std::string_view line) {
            lucent::info("wine", "test: {}", line);
            if (line.find("Microsoft Windows") != std::string_view::npos) {
                answer = line;
            }
            if (!line.empty()) {
                last = line;
            }
        },
        stop);
    std::string result;
    if (status && *status == 0 && !answer.empty()) {
        result = "Works: " + answer;
    } else {
        result = runner + " failed" +
                 (status ? " (exit " + std::to_string(*status) + ")" : std::string{}) +
                 (last.empty() ? std::string{} : ": " + last);
    }
    lucent::info("wine", "test of {}: {}", runner, result);
    const std::lock_guard lock{mutex_};
    look_.testing = false;
    look_.lastTest = result;
    news_.push_back(Finished{.serial = 0,
                             .id = "test",
                             .name = runner,
                             .outcome = answer.empty() ? Outcome::Failed : Outcome::Installed,
                             .text = result});
}

void WineRunners::measure() {
    Look measured;
    for (const library::ge_proton::Build& build :
         library::ge_proton::installed(library::ge_proton::runnersDir(dataDir_))) {
        measured.builds.push_back(
            Piece{.name = build.name, .dir = build.dir, .bytes = bytesUnder(build.dir)});
    }
    const std::vector<library::umu::Install> umu =
        library::umu::installed(library::umu::launcherDir(dataDir_));
    if (!umu.empty()) {
        const fs::path dir = umu.front().umuRun.parent_path().parent_path();
        measured.umu = Piece{
            .name = "umu-launcher " + umu.front().version, .dir = dir, .bytes = bytesUnder(dir)};
    }
    if (const fs::path runtime = library::umu::steamRuntime(library::wine::umuFolders(dataDir_));
        !runtime.empty()) {
        measured.steamRuntime =
            Piece{.name = "Steam Linux Runtime (" + runtime.filename().string() + ")",
                  .dir = runtime,
                  .bytes = bytesUnder(runtime)};
    }
    const std::lock_guard lock{mutex_};
    look_.builds = std::move(measured.builds);
    look_.umu = std::move(measured.umu);
    look_.steamRuntime = std::move(measured.steamRuntime);
}

void WineRunners::ended(Outcome outcome, const std::string& id, const std::string& name,
                        std::string text) {
    const std::lock_guard lock{mutex_};
    finished_ = Finished{
        .serial = ++serial_, .id = id, .name = name, .outcome = outcome, .text = std::move(text)};
    news_.push_back(*finished_);
}

void WineRunners::service(Clock::time_point now) {
    std::vector<Finished> news;
    bool checked = false;
    std::optional<int> percent;
    bool busyNow = false;
    {
        const std::lock_guard lock{mutex_};
        news = std::exchange(news_, {});
        checked = std::exchange(checked_, false);
        busyNow = running_;
        if (progress_ && progress_->total > 0) {
            percent = static_cast<int>(progress_->received * fullPercent / progress_->total);
        }
    }
    for (const Finished& finished : news) {
        hooks_.say(finished.text, finished.outcome == Outcome::Failed);
    }
    if (checked) {
        rotate();
    }
    if (!news.empty() || checked) {
        hooks_.changed();
    }
    if (percent != shownPercent_) {
        shownPercent_ = percent;
        hooks_.refresh();
    }
    if (!nextCheck_) {
        nextCheck_ = now;
    }
    if (now >= *nextCheck_ && !busyNow && hooks_.wanted()) {
        lucent::info("wine", "checking for a newer GE-Proton and umu-launcher");
        nextCheck_ = now + (checkNow().empty() ? checkEvery : retryFailedAfter);
    }
}

void WineRunners::rotate() {
    const fs::path runners = library::ge_proton::runnersDir(dataDir_);
    const std::vector<library::ge_proton::Build> builds = library::ge_proton::installed(runners);
    const InUse inUse = hooks_.inUse();
    std::set<fs::path> keep;
    for (const auto& [dir, why] : inUse) {
        keep.insert(dir);
    }
    std::map<std::string, std::string> kept;
    for (std::size_t at = library::ge_proton::kept; at < builds.size(); ++at) {
        if (const auto used = inUse.find(builds[at].dir); used != inUse.end()) {
            kept[builds[at].name] = used->second;
            lucent::info("wine", "{} stays past the newest two: {}", builds[at].name, used->second);
        }
    }
    bool removed = false;
    for (const library::ge_proton::Build& build : library::ge_proton::rotation(builds, keep)) {
        lucent::info("wine", "rotation removes {} (keeping {} and {})", build.name, builds[0].name,
                     builds[1].name);
        removed = discard(dataDir_, build.dir, build.name) || removed;
    }
    if (!hooks_.gameRunning()) {
        const std::vector<library::umu::Install> umu =
            library::umu::installed(library::umu::launcherDir(dataDir_));
        for (std::size_t at = 1; at < umu.size(); ++at) {
            lucent::info("wine", "removing the older umu-launcher {}", umu[at].version);
            removed = discard(dataDir_, umu[at].umuRun.parent_path().parent_path(),
                              "umu-launcher-" + umu[at].version) ||
                      removed;
        }
    }
    {
        const std::lock_guard lock{mutex_};
        look_.kept = std::move(kept);
        std::erase_if(look_.builds, [](const Piece& piece) {
            std::error_code ec;
            return !fs::exists(piece.dir, ec);
        });
    }
    if (removed) {
        sweep();
    }
}

void WineRunners::sweep() {
    if (sweeper_.joinable()) {
        sweeper_.join();
    }
    sweeper_ = std::jthread{[trash = trashDir(dataDir_)] {
        std::error_code ec;
        fs::remove_all(trash, ec);
        if (ec) {
            lucent::warn("wine", "cannot empty {}: {}", trash.string(), ec.message());
        }
    }};
}

} // namespace opensu::app

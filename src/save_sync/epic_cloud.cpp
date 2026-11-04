#include "epic_cloud.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "clock/iso_time.hpp"
#include "fileio/atomic_write.hpp"
#include "json_text.hpp"
#include "launch/command.hpp"
#include "library/save_copy.hpp"
#include "lucent/log.h"
#include "save_bundle.hpp"

namespace opensu::save_sync {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;
using Standing = TargetSaves::Standing;

constexpr std::string_view targetId = "epic";
constexpr std::string_view targetName = "Epic";
constexpr std::string_view manifestEnd = ".manifest";

/// The files under `folder` as a shared bundle counts them: their content hash, bytes and newest
/// time; no hash when it holds none.
SaveStatus statusOf(const fs::path& folder) {
    std::error_code ec;
    if (folder.empty() || !fs::is_directory(folder, ec)) {
        return {};
    }
    library::saves::EmulatorSaves saves;
    saves.emulatorName = std::string{targetName};
    saves.slots.push_back(library::saves::SaveSlot{.kind = library::saves::SlotKind::Account,
                                                   .user = {},
                                                   .current = true,
                                                   .root = folder,
                                                   .content = {folder},
                                                   .format = library::saves::SaveFormat::AsIs,
                                                   .bytes = 0,
                                                   .modified = std::chrono::system_clock::now()});
    std::string error;
    const std::optional<SaveBundle> bundle = bundleOf(saves, error);
    if (!bundle || bundle->files.empty()) {
        return {};
    }
    const auto newest = bundle->modified().value_or(std::chrono::system_clock::time_point{});
    return SaveStatus{
        .counter = 0,
        .hash = bundle->hash(),
        .bytes = bundle->bytes(),
        .modified = std::chrono::floor<std::chrono::seconds>(newest).time_since_epoch().count()};
}

} // namespace

std::string newestCloudSave(std::string_view output) {
    std::string newest;
    while (!output.empty()) {
        std::string_view line = output.substr(0, output.find('\n'));
        output.remove_prefix(std::min(output.size(), line.size() + 1));
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.remove_suffix(1);
        }
        // " + 2026.11.04-18.09.00.manifest"
        if (!line.starts_with(" + ") || !line.ends_with(manifestEnd)) {
            continue;
        }
        line.remove_prefix(3);
        if (line > newest) {
            newest = std::string{line};
        }
    }
    return newest;
}

std::optional<std::int64_t> cloudSaveTime(std::string_view manifest) {
    // YYYY.MM.DD-HH.MM.SS.manifest, as legendary names an upload by its local save's UTC time.
    constexpr std::size_t stampSize = 19;
    if (manifest.size() != stampSize + manifestEnd.size() || !manifest.ends_with(manifestEnd)) {
        return std::nullopt;
    }
    std::string iso{manifest.substr(0, stampSize)};
    if (iso[4] != '.' || iso[7] != '.' || iso[10] != '-' || iso[13] != '.' || iso[16] != '.') {
        return std::nullopt;
    }
    iso[4] = '-';
    iso[7] = '-';
    iso[10] = 'T';
    iso[13] = ':';
    iso[16] = ':';
    return clock::parseIsoTime(iso);
}

EpicCloud::EpicCloud(Setup setup) : setup_{std::move(setup)} {
    std::error_code ec;
    if (fs::is_regular_file(setup_.recordFile, ec)) {
        std::ifstream in{setup_.recordFile};
        const json document = json::parse(in, nullptr, false);
        if (document.is_object()) {
            for (const auto& [game, entry] : document.items()) {
                if (entry.is_object()) {
                    record_.insert_or_assign(game,
                                             Synced{.hash = textField(entry, "hash"),
                                                    .manifest = textField(entry, "manifest")});
                }
            }
        }
    }
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        work(stop);
    }};
}

EpicCloud::~EpicCloud() {
    worker_.request_stop();
    wake_.notify_all();
}

bool EpicCloud::serves(const library::Game& game) {
    return game.source == library::Source::Epic && game.installed && !game.sourceId.empty();
}

std::uint64_t EpicCloud::prepare(const library::Game& game) {
    std::uint64_t ticket = 0;
    {
        const std::lock_guard lock{mutex_};
        ticket = ++issued_;
    }
    queue(Job{.game = game, .pick = Pick::None, .lookOnly = false, .ticket = ticket});
    return ticket;
}

bool EpicCloud::prepared(std::uint64_t ticket) const {
    const std::lock_guard lock{mutex_};
    return finished_.contains(ticket);
}

void EpicCloud::sync(const library::Game& game) {
    queue(Job{.game = game, .pick = Pick::None, .lookOnly = false, .ticket = 0});
}

void EpicCloud::look(const library::Game& game) {
    queue(Job{.game = game, .pick = Pick::None, .lookOnly = true, .ticket = 0});
}

void EpicCloud::keep(const library::Game& game, bool theirs) {
    queue(Job{
        .game = game, .pick = theirs ? Pick::Theirs : Pick::Ours, .lookOnly = false, .ticket = 0});
}

std::optional<TargetSaves> EpicCloud::standing(const std::string& gameId) const {
    const std::lock_guard lock{mutex_};
    const auto found = standings_.find(gameId);
    return found == standings_.end() ? std::nullopt : std::optional{found->second};
}

bool EpicCloud::busy(const std::string& gameId) const {
    const std::lock_guard lock{mutex_};
    return running_.contains(gameId) || std::ranges::any_of(jobs_, [&gameId](const Job& job) {
               return job.game.id == gameId && !job.lookOnly;
           });
}

void EpicCloud::queue(Job job) {
    if (!serves(job.game)) {
        if (job.ticket != 0) {
            const std::lock_guard lock{mutex_};
            finished_.insert(job.ticket);
        }
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        jobs_.push_back(std::move(job));
    }
    wake_.notify_all();
}

void EpicCloud::work(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Job job;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !jobs_.empty();
                })) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
            running_.insert(job.game.id);
        }
        TargetSaves standing = run(job);
        {
            const std::lock_guard lock{mutex_};
            standings_.insert_or_assign(job.game.id, std::move(standing));
            running_.erase(job.game.id);
            if (job.ticket != 0) {
                finished_.insert(job.ticket);
            }
        }
        setup_.changed();
    }
}

std::optional<std::string> EpicCloud::legendary(const std::vector<std::string>& arguments,
                                                std::string& error) const {
    const std::optional<launch::Captured> result =
        launch::runCaptured(setup_.legendary, arguments, launch::CaptureErrors::Merged);
    if (!result) {
        error = "legendary is not installed";
        return std::nullopt;
    }
    if (result->status != 0) {
        error = "legendary " + arguments.front() + " failed (status " +
                std::to_string(result->status) + ")";
        return std::nullopt;
    }
    return result->output;
}

fs::path EpicCloud::saveFolder(const std::string& app, std::string& error) const {
    const auto kept = [this, &app] {
        const std::optional<launch::Captured> info =
            launch::runCaptured(setup_.legendary, {"info", app, "--json"});
        if (!info || info->status != 0) {
            return fs::path{};
        }
        const json document = json::parse(info->output, nullptr, false);
        if (!document.is_object() || !document.contains("install") ||
            !document["install"].is_object()) {
            return fs::path{};
        }
        return fs::path{textField(document["install"], "save_path")};
    };
    if (fs::path folder = kept(); !folder.empty()) {
        return folder;
    }
    if (!legendary({"sync-saves", app, "--skip-upload", "--skip-download", "-y", "--accept-path"},
                   error)) {
        return {};
    }
    fs::path folder = kept();
    if (folder.empty()) {
        error = "legendary knows no save folder of it";
    }
    return folder;
}

std::optional<std::string> EpicCloud::newestOf(const std::string& app, std::string& error) const {
    const std::optional<std::string> listed = legendary({"list-saves", app}, error);
    return listed ? std::optional{newestCloudSave(*listed)} : std::nullopt;
}

TargetSaves EpicCloud::run(const Job& job) {
    const std::string& app = job.game.sourceId;
    TargetSaves standing{.targetId = std::string{targetId},
                         .targetName = std::string{targetName},
                         .standing = Standing::Failed,
                         .ours = {},
                         .theirs = std::nullopt,
                         .problem = {}};
    std::string error;
    const fs::path folder = saveFolder(app, error);
    const std::optional<std::string> cloud = folder.empty() ? std::nullopt : newestOf(app, error);
    if (!cloud) {
        standing.problem = error;
        lucent::warn("saves", "{}: Epic's cloud cannot be asked: {}", job.game.title, error);
        return standing;
    }
    const auto theirsOf = [](const std::string& manifest) {
        return SaveStatus{.counter = 0,
                          .hash = manifest,
                          .bytes = 0,
                          .modified = cloudSaveTime(manifest).value_or(0)};
    };
    const SaveStatus ours = statusOf(folder);
    standing.ours = ours;
    standing.theirs = theirsOf(*cloud);
    Synced last;
    {
        const std::lock_guard lock{mutex_};
        if (const auto found = record_.find(job.game.id); found != record_.end()) {
            last = found->second;
        }
    }
    SyncState sync = SyncState::InSync;
    if (ours.hash.empty() != cloud->empty()) {
        sync = ours.hash.empty() ? SyncState::Pull : SyncState::Push;
    } else if (!ours.hash.empty()) {
        sync = stateOf(ours.hash != last.hash, *cloud != last.manifest);
    }
    if (job.pick == Pick::Theirs && !cloud->empty()) {
        sync = SyncState::Pull;
    } else if (job.pick == Pick::Ours && !ours.hash.empty()) {
        sync = SyncState::Push;
    }
    const auto record = [this, &job](const std::string& hash, const std::string& manifest) {
        const std::lock_guard lock{mutex_};
        record_.insert_or_assign(job.game.id, Synced{.hash = hash, .manifest = manifest});
        keep();
    };
    switch (sync) {
    case SyncState::InSync:
        record(ours.hash, *cloud);
        standing.standing = Standing::InSync;
        return standing;
    case SyncState::Conflict:
        standing.standing = Standing::Conflict;
        return standing;
    case SyncState::Pull:
        standing.standing = Standing::NewerThere;
        break;
    case SyncState::Push:
        standing.standing = Standing::NewerHere;
        break;
    }
    if (job.lookOnly) {
        return standing;
    }
    const bool pull = sync == SyncState::Pull;
    if (pull && !ours.hash.empty()) {
        const fs::path backup = library::saves::newBackupFolder(
            setup_.backups / library::saves::backupsFolder / "epic" / app,
            std::chrono::system_clock::now());
        std::error_code ec;
        fs::create_directories(backup, ec);
        fs::copy(folder, backup, fs::copy_options::recursive, ec);
        if (ec) {
            standing.standing = Standing::Failed;
            standing.problem = "the save folder cannot be backed up: " + ec.message();
            return standing;
        }
        lucent::info("saves", "{}: the save folder backed up in {}", job.game.title,
                     backup.string());
    }
    std::optional<std::string> now;
    if (legendary({"sync-saves", app, pull ? "--force-download" : "--force-upload", "-y",
                   "--accept-path"},
                  error)) {
        now = pull ? cloud : newestOf(app, error);
    }
    if (!now) {
        standing.standing = Standing::Failed;
        standing.problem = error;
        lucent::warn("saves", "{}: Epic's cloud saves cannot be {}: {}", job.game.title,
                     pull ? "taken" : "given", error);
        return standing;
    }
    standing.ours = statusOf(folder);
    standing.theirs = theirsOf(*now);
    record(standing.ours.hash, *now);
    lucent::info("saves", "{}: {} Epic's cloud", job.game.title,
                 pull ? "took the saves from" : "gave the saves to");
    standing.standing = Standing::InSync;
    return standing;
}

void EpicCloud::keep() const {
    json document = json::object();
    for (const auto& [game, synced] : record_) {
        document[game] = {{"hash", synced.hash}, {"manifest", synced.manifest}};
    }
    std::string error;
    if (!fileio::writeWhole(setup_.recordFile, document.dump(2) + "\n", error)) {
        lucent::warn("saves", "the record of Epic's cloud saves cannot be kept: {}", error);
    }
}

} // namespace opensu::save_sync

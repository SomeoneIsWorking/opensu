#include "save_sync.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>
#include <utility>

#include "lucent/log.h"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::save_sync {
namespace {

namespace fs = std::filesystem;
using library::saves::SaveKey;
using Standing = TargetSaves::Standing;

/// How much of a hash names a staging folder.
constexpr std::size_t stagingNameBytes = 16;

std::int64_t unixOf(const std::optional<std::chrono::system_clock::time_point>& at) {
    return at ? std::chrono::floor<std::chrono::seconds>(*at).time_since_epoch().count() : 0;
}

std::string shortHash(const std::string& text) {
    return secrets::hexText(secrets::crypto::sha256(text)).substr(0, stagingNameBytes * 2);
}

std::string standingsKey(const std::string& profile, const std::string& gameId) {
    return profile + "\n" + gameId;
}

TargetSaves standingOf(const SaveTarget& target, Standing standing, const SaveStatus& ours) {
    return TargetSaves{.targetId = target.id(),
                       .targetName = target.name(),
                       .standing = standing,
                       .ours = ours,
                       .theirs = std::nullopt,
                       .problem = {}};
}

} // namespace

SaveSync::SaveSync(Setup setup)
    : setup_{std::move(setup)}, versions_{setup_.versionsFile}, copier_{setup_.backups} {
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        work(stop);
    }};
}

SaveSync::~SaveSync() {
    stop();
}

void SaveSync::stop() {
    worker_.request_stop();
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SaveSync::know(const std::vector<library::Game>& games,
                    library::roms::EmulatorCatalog catalog) {
    std::vector<library::Game> keyed;
    for (const library::Game& game : games) {
        if (game.installed && library::saves::keyOf(game, {})) {
            keyed.push_back(game);
        }
    }
    const std::lock_guard lock{mutex_};
    games_ = std::move(keyed);
    catalog_ = std::move(catalog);
}

void SaveSync::setTargets(const std::string& group, Targets targets) {
    {
        const std::lock_guard lock{mutex_};
        if (targets) {
            groups_.insert_or_assign(group, std::move(targets));
        } else {
            groups_.erase(group);
        }
    }
    const std::lock_guard between{job_};
}

std::vector<std::shared_ptr<SaveTarget>> SaveSync::targets() const {
    std::vector<Targets> groups;
    {
        const std::lock_guard lock{mutex_};
        for (const auto& [name, listed] : groups_) {
            groups.push_back(listed);
        }
    }
    std::vector<std::shared_ptr<SaveTarget>> all;
    for (const Targets& listed : groups) {
        std::ranges::move(listed(), std::back_inserter(all));
    }
    return all;
}

std::optional<library::Game> SaveSync::gameOf(const SaveKey& key) const {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find_if(games_, [&key](const library::Game& game) {
        const std::optional<SaveKey> its = library::saves::keyOf(game, key.profile);
        return its && its->system == key.system && its->game == key.game;
    });
    if (found == games_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<SaveSync::Local> SaveSync::localOf(const library::Game& game,
                                                 const std::string& profile, std::string& error) {
    const std::optional<SaveKey> key = library::saves::keyOf(game, profile);
    if (!key) {
        error = game.title + " keeps no saves openSU can find";
        return std::nullopt;
    }
    const std::optional<std::size_t> seat = setup_.seatOf(profile);
    if (!seat) {
        error = "the profile is not here";
        return std::nullopt;
    }
    library::roms::EmulatorCatalog catalog;
    {
        const std::lock_guard lock{mutex_};
        catalog = catalog_;
    }
    Local local{.key = *key,
                .all = library::saves::savesOf(game, catalog, setup_.runnerSaves, profile, *seat),
                .launched = {},
                .bundle = {},
                .status = {}};
    const library::saves::EmulatorSaves* launched =
        library::saves::launchedSaves(local.all, game, catalog);
    if (launched == nullptr) {
        error = "the emulator " + game.title + " starts with keeps no saves openSU can find";
        return std::nullopt;
    }
    local.launched = *launched;
    std::optional<SaveBundle> bundle = bundleOf(*launched, error);
    if (!bundle) {
        return std::nullopt;
    }
    local.bundle = std::move(*bundle);
    const std::string hash = local.bundle.files.empty() ? std::string{} : local.bundle.hash();
    const SaveVersion version = versions_.observe(local.key, hash);
    local.status = SaveStatus{.counter = version.counter,
                              .hash = hash,
                              .bytes = local.bundle.bytes(),
                              .modified = unixOf(local.bundle.modified())};
    return local;
}

std::optional<SaveStatus> SaveSync::statusOf(const SaveKey& key) {
    const std::optional<library::Game> game = gameOf(key);
    if (!game) {
        return std::nullopt;
    }
    const std::lock_guard lock{work_};
    std::string error;
    const std::optional<Local> local = localOf(*game, key.profile, error);
    if (!local) {
        lucent::warn("saves", "{}'s saves cannot be read for a target: {}", game->title, error);
        return std::nullopt;
    }
    return local->status;
}

std::optional<std::string> SaveSync::sendable(const SaveKey& key, std::string& error) {
    const std::optional<library::Game> game = gameOf(key);
    if (!game) {
        error = "no game here has those saves";
        return std::nullopt;
    }
    const std::lock_guard lock{work_};
    const std::optional<Local> local = localOf(*game, key.profile, error);
    if (!local) {
        return std::nullopt;
    }
    return write(local->bundle);
}

void SaveSync::heard(const std::string& targetId, const SaveKey& key, bool picked) {
    std::optional<library::Game> game = gameOf(key);
    if (!game) {
        return;
    }
    queue(Job{.game = std::move(*game),
              .profile = key.profile,
              .targetId = targetId,
              .pick = picked ? Pick::Theirs : Pick::None,
              .lookOnly = false,
              .ticket = 0});
}

void SaveSync::syncedWith(const std::string& targetId, const SaveKey& key,
                          const std::string& hash) {
    std::optional<library::Game> game = gameOf(key);
    if (!game || hash.empty()) {
        return;
    }
    versions_.synced(key, targetId, hash);
    lucent::info("saves", "{} took the saves of {} from here", targetId, game->title);
    queue(Job{.game = std::move(*game),
              .profile = key.profile,
              .targetId = targetId,
              .pick = Pick::None,
              .lookOnly = true,
              .ticket = 0});
}

std::uint64_t SaveSync::prepare(const library::Game& game, const std::string& profile) {
    std::uint64_t ticket = 0;
    {
        const std::lock_guard lock{mutex_};
        ticket = ++issued_;
    }
    queue(Job{.game = game,
              .profile = profile,
              .targetId = {},
              .pick = Pick::None,
              .lookOnly = false,
              .ticket = ticket});
    return ticket;
}

bool SaveSync::prepared(std::uint64_t ticket) const {
    const std::lock_guard lock{mutex_};
    return finished_.contains(ticket);
}

void SaveSync::sync(const library::Game& game, const std::string& profile) {
    queue(Job{.game = game,
              .profile = profile,
              .targetId = {},
              .pick = Pick::None,
              .lookOnly = false,
              .ticket = 0});
}

void SaveSync::syncAll(const std::string& profile) {
    std::vector<library::Game> games;
    {
        const std::lock_guard lock{mutex_};
        games = games_;
    }
    for (const library::Game& game : games) {
        sync(game, profile);
    }
}

void SaveSync::look(const library::Game& game, const std::string& profile) {
    queue(Job{.game = game,
              .profile = profile,
              .targetId = {},
              .pick = Pick::None,
              .lookOnly = true,
              .ticket = 0});
}

bool SaveSync::sharing(const library::Game& game) const {
    if (!library::saves::keyOf(game, {})) {
        return false;
    }
    return std::ranges::any_of(targets(), [](const std::shared_ptr<SaveTarget>& target) {
        return target->reachable();
    });
}

void SaveSync::keep(const library::Game& game, const std::string& profile,
                    const std::string& targetId, bool theirs) {
    queue(Job{.game = game,
              .profile = profile,
              .targetId = targetId,
              .pick = theirs ? Pick::Theirs : Pick::Ours,
              .lookOnly = false,
              .ticket = 0});
}

std::vector<TargetSaves> SaveSync::standings(const std::string& gameId,
                                             const std::string& profile) const {
    const std::lock_guard lock{mutex_};
    const auto found = standings_.find(standingsKey(profile, gameId));
    return found == standings_.end() ? std::vector<TargetSaves>{} : found->second;
}

bool SaveSync::busy(const std::string& gameId) const {
    const std::lock_guard lock{mutex_};
    return running_.contains(gameId) || std::ranges::any_of(jobs_, [&gameId](const Job& job) {
               return job.game.id == gameId && !job.lookOnly;
           });
}

void SaveSync::queue(Job job) {
    {
        const std::lock_guard lock{mutex_};
        jobs_.push_back(std::move(job));
    }
    wake_.notify_all();
}

void SaveSync::work(const std::stop_token& stop) {
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
            if (!job.lookOnly) {
                running_.insert(job.game.id);
            }
        }
        {
            const std::lock_guard running{job_};
            run(job);
        }
        {
            const std::lock_guard lock{mutex_};
            running_.erase(job.game.id);
            if (job.ticket != 0) {
                finished_.insert(job.ticket);
            }
        }
        setup_.changed();
    }
}

void SaveSync::run(const Job& job) {
    std::vector<std::shared_ptr<SaveTarget>> found = targets();
    std::erase_if(found, [&job](const std::shared_ptr<SaveTarget>& target) {
        return !job.targetId.empty() && target->id() != job.targetId;
    });
    if (found.empty() || !library::saves::keyOf(job.game, job.profile)) {
        const std::lock_guard lock{mutex_};
        if (job.targetId.empty()) {
            standings_.erase(standingsKey(job.profile, job.game.id));
        }
        return;
    }
    SaveStatus ours;
    {
        const std::lock_guard lock{work_};
        std::string error;
        if (const std::optional<Local> local = localOf(job.game, job.profile, error)) {
            ours = local->status;
        }
    }
    std::vector<TargetSaves> stood;
    for (const std::shared_ptr<SaveTarget>& target : found) {
        if (!target->reachable()) {
            stood.push_back(standingOf(*target, Standing::Offline, ours));
            continue;
        }
        if (std::optional<TargetSaves> standing = syncWith(job, *target)) {
            stood.push_back(std::move(*standing));
        }
    }
    const std::lock_guard lock{mutex_};
    std::vector<TargetSaves>& kept = standings_[standingsKey(job.profile, job.game.id)];
    if (job.targetId.empty()) {
        kept = std::move(stood);
        return;
    }
    for (TargetSaves& standing : stood) {
        const auto same = std::ranges::find(kept, standing.targetId, &TargetSaves::targetId);
        if (same != kept.end()) {
            *same = std::move(standing);
        } else {
            kept.push_back(std::move(standing));
        }
    }
}

std::optional<TargetSaves> SaveSync::syncWith(const Job& job, SaveTarget& target) {
    TargetSaves standing = standingOf(target, Standing::Failed, {});
    std::string error;
    std::optional<Local> local;
    {
        const std::lock_guard lock{work_};
        local = localOf(job.game, job.profile, error);
    }
    if (!local) {
        standing.problem = error;
        return standing;
    }
    standing.ours = local->status;
    const SaveTarget::Held held = target.status(local->key);
    switch (held.kind) {
    case SaveTarget::Held::Kind::NoGame:
        return std::nullopt;
    case SaveTarget::Held::Kind::NotShared:
        standing.standing = Standing::NotShared;
        return standing;
    case SaveTarget::Held::Kind::Failed:
        standing.problem = held.problem;
        lucent::warn("saves", "{} {}: {}", local->key.system, local->key.game, standing.problem);
        return standing;
    case SaveTarget::Held::Kind::Saves:
        break;
    }
    const SaveStatus& theirs = held.status;
    standing.theirs = theirs;
    const std::string synced = versions_.syncedWith(local->key, target.id());
    SyncState state = syncStateOf(local->status.hash, theirs.hash, synced);
    if (state == SyncState::InSync) {
        versions_.synced(local->key, target.id(), local->status.hash);
    } else if (job.pick == Pick::Theirs && !theirs.hash.empty()) {
        state = SyncState::Pull;
    } else if (job.pick == Pick::Ours && !local->status.hash.empty()) {
        state = SyncState::Push;
    }
    switch (state) {
    case SyncState::InSync:
        standing.standing = Standing::InSync;
        return standing;
    case SyncState::Conflict:
        standing.standing = Standing::Conflict;
        return standing;
    case SyncState::Push: {
        standing.standing = Standing::NewerHere;
        if (job.lookOnly) {
            return standing;
        }
        const SaveTarget::Given given =
            target.give(local->key, local->bundle, local->status, job.pick == Pick::Ours, error);
        if (given == SaveTarget::Given::Failed) {
            standing.standing = Standing::Failed;
            standing.problem = error;
            lucent::warn("saves", "{} {}: {}", local->key.system, local->key.game, error);
            return standing;
        }
        if (given == SaveTarget::Given::Kept) {
            versions_.synced(local->key, target.id(), local->status.hash);
            standing.standing = Standing::InSync;
            standing.theirs = local->status;
        }
        lucent::info("saves", "{} {}: {} {} the newer saves here (version {})", local->key.system,
                     local->key.game, target.name(),
                     given == SaveTarget::Given::Kept ? "holds" : "was told of",
                     local->status.counter);
        return standing;
    }
    case SyncState::Pull:
        break;
    }
    standing.standing = Standing::NewerThere;
    if (job.lookOnly) {
        return standing;
    }
    const std::optional<SaveBundle> bundle = target.fetch(local->key, error);
    if (!bundle || bundle->hash() != theirs.hash) {
        standing.standing = Standing::Failed;
        standing.problem = bundle ? target.name() + "'s saves changed while they were taken"
                                  : target.name() + "'s saves cannot be had: " + error;
        lucent::warn("saves", "{} {}: {}", local->key.system, local->key.game, standing.problem);
        return standing;
    }
    if (std::string refused = take(job, target, *bundle, theirs, local->status.hash);
        !refused.empty()) {
        standing.standing = Standing::Failed;
        standing.problem = std::move(refused);
        lucent::warn("saves", "{} {}: {}'s saves not taken: {}", local->key.system, local->key.game,
                     target.name(), standing.problem);
        return standing;
    }
    standing.standing = Standing::InSync;
    standing.ours = theirs;
    target.took(local->key, theirs);
    return standing;
}

std::string SaveSync::take(const Job& job, const SaveTarget& target, const SaveBundle& bundle,
                           const SaveStatus& theirs, const std::string& expected) {
    const library::Game& game = job.game;
    const std::lock_guard lock{work_};
    std::string error;
    const std::optional<Local> local = localOf(game, job.profile, error);
    if (!local) {
        return error;
    }
    if (local->status.hash != expected) {
        return "the saves here changed meanwhile";
    }
    if (std::string refused = setup_.inUse(game, local->launched); !refused.empty()) {
        return refused;
    }
    const fs::path folder =
        setup_.staging / shortHash(target.id()) /
        shortHash(local->key.profile + "\n" + local->key.system + "\n" + local->key.game);
    library::saves::CopyProgress progress;
    try {
        const library::saves::EmulatorSaves from = staged(bundle, folder, target.name());
        error = copier_.copy(local->key, from, local->launched, std::chrono::system_clock::now(),
                             progress);
    } catch (const fs::filesystem_error& failure) {
        error = "they cannot be staged: " + std::string{failure.what()};
    }
    std::error_code ec;
    fs::remove_all(folder, ec);
    if (!error.empty()) {
        return error;
    }
    versions_.took(local->key, theirs.hash, theirs.counter);
    versions_.synced(local->key, target.id(), theirs.hash);
    lucent::info("saves", "{} {}: took {}'s saves (version {}, {} bytes) over {}'s",
                 local->key.system, local->key.game, target.name(), theirs.counter, theirs.bytes,
                 local->launched.emulatorName);
    return {};
}

} // namespace opensu::save_sync

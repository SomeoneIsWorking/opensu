#include "save_sync.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>
#include <utility>

#include "lucent/log.h"
#include "peer_client.hpp"
#include "peer_crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using library::saves::SaveKey;
using Standing = PeerSaves::Standing;

constexpr long ok = 200;
constexpr long forbidden = 403;
constexpr long notFound = 404;
/// A status, a notice or a record is a few hundred bytes.
constexpr std::size_t largestNote = std::size_t{16} * 1024;
/// How much of the key's hash names its staging folder.
constexpr std::size_t stagingNameBytes = 16;

std::int64_t unixOf(const std::optional<std::chrono::system_clock::time_point>& at) {
    return at ? std::chrono::floor<std::chrono::seconds>(*at).time_since_epoch().count() : 0;
}

wire::SaveAsk askFor(const SaveKey& key) {
    return wire::SaveAsk{
        .system = key.system, .game = key.game, .picked = false, .hash = {}, .counter = 0};
}

std::string describe(const std::optional<net::WebClient::Answer>& answer,
                     const std::string& error) {
    return answer ? "HTTP " + std::to_string(answer->status) : error;
}

} // namespace

SaveSync::SaveSync(Setup setup, PeerBook& book, const net::WebClient& web,
                   std::function<std::vector<Sighting>()> online)
    : setup_{std::move(setup)}, book_{book}, web_{web}, online_{std::move(online)},
      versions_{setup_.versionsFile}, copier_{setup_.backups} {
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
        if (game.installed && library::saves::keyOf(game)) {
            keyed.push_back(game);
        }
    }
    const std::lock_guard lock{mutex_};
    games_ = std::move(keyed);
    catalog_ = std::move(catalog);
}

std::optional<library::Game> SaveSync::gameOf(const SaveKey& key) const {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find_if(games_, [&key](const library::Game& game) {
        return library::saves::keyOf(game) == key;
    });
    if (found == games_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<SaveSync::Local> SaveSync::localOf(const library::Game& game, std::string& error) {
    const std::optional<SaveKey> key = library::saves::keyOf(game);
    if (!key) {
        error = game.title + " keeps no saves openSU can find";
        return std::nullopt;
    }
    library::roms::EmulatorCatalog catalog;
    {
        const std::lock_guard lock{mutex_};
        catalog = catalog_;
    }
    Local local{.key = *key,
                .all = library::saves::savesOf(game, catalog, setup_.runnerSaves),
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
    local.status = wire::SaveStatus{.counter = version.counter,
                                    .hash = hash,
                                    .bytes = local.bundle.bytes(),
                                    .modified = unixOf(local.bundle.modified())};
    return local;
}

std::optional<wire::SaveStatus> SaveSync::statusOf(const SaveKey& key) {
    const std::optional<library::Game> game = gameOf(key);
    if (!game) {
        return std::nullopt;
    }
    const std::lock_guard lock{work_};
    std::string error;
    const std::optional<Local> local = localOf(*game, error);
    if (!local) {
        lucent::warn("saves", "{}'s saves cannot be read for a peer: {}", game->title, error);
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
    const std::optional<Local> local = localOf(*game, error);
    if (!local) {
        return std::nullopt;
    }
    return write(local->bundle);
}

void SaveSync::heard(const Peer& peer, const wire::SaveAsk& ask) {
    std::optional<library::Game> game = gameOf(SaveKey{ask.system, ask.game});
    if (!game) {
        return;
    }
    lucent::info("saves", "{} has newer saves of {}{}", peer.name, game->title,
                 ask.picked ? ", picked by the player there" : "");
    queue(Job{.game = std::move(*game),
              .peerId = peer.id,
              .pick = ask.picked ? Pick::Theirs : Pick::None,
              .lookOnly = false,
              .ticket = 0});
}

void SaveSync::syncedWith(const Peer& peer, const wire::SaveAsk& ask) {
    std::optional<library::Game> game = gameOf(SaveKey{ask.system, ask.game});
    if (!game || ask.hash.empty()) {
        return;
    }
    versions_.synced(SaveKey{ask.system, ask.game}, peer.id, ask.hash);
    lucent::info("saves", "{} took the saves of {} from here", peer.name, game->title);
    queue(Job{.game = std::move(*game),
              .peerId = peer.id,
              .pick = Pick::None,
              .lookOnly = true,
              .ticket = 0});
}

std::uint64_t SaveSync::prepare(const library::Game& game) {
    std::uint64_t ticket = 0;
    {
        const std::lock_guard lock{mutex_};
        ticket = ++issued_;
    }
    queue(Job{.game = game, .peerId = {}, .pick = Pick::None, .lookOnly = false, .ticket = ticket});
    return ticket;
}

bool SaveSync::prepared(std::uint64_t ticket) const {
    const std::lock_guard lock{mutex_};
    return finished_.contains(ticket);
}

void SaveSync::sync(const library::Game& game) {
    queue(Job{.game = game, .peerId = {}, .pick = Pick::None, .lookOnly = false, .ticket = 0});
}

void SaveSync::look(const library::Game& game) {
    queue(Job{.game = game, .peerId = {}, .pick = Pick::None, .lookOnly = true, .ticket = 0});
}

bool SaveSync::sharing(const library::Game& game) const {
    if (!library::saves::keyOf(game)) {
        return false;
    }
    const std::vector<Sighting> seen = online_();
    return std::ranges::any_of(book_.paired(), [&seen](const Peer& peer) {
        return peer.sharesSaves && std::ranges::find(seen, peer.id, &Sighting::id) != seen.end();
    });
}

void SaveSync::keep(const library::Game& game, const std::string& peerId, bool theirs) {
    queue(Job{.game = game,
              .peerId = peerId,
              .pick = theirs ? Pick::Theirs : Pick::Ours,
              .lookOnly = false,
              .ticket = 0});
}

std::vector<PeerSaves> SaveSync::standings(const std::string& gameId) const {
    const std::lock_guard lock{mutex_};
    const auto found = standings_.find(gameId);
    return found == standings_.end() ? std::vector<PeerSaves>{} : found->second;
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
        run(job);
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
    std::vector<Peer> peers = book_.paired();
    std::erase_if(peers, [&job](const Peer& peer) {
        return !peer.sharesSaves || (!job.peerId.empty() && peer.id != job.peerId);
    });
    if (peers.empty() || !library::saves::keyOf(job.game)) {
        const std::lock_guard lock{mutex_};
        if (job.peerId.empty()) {
            standings_.erase(job.game.id);
        }
        return;
    }
    wire::SaveStatus ours;
    {
        const std::lock_guard lock{work_};
        std::string error;
        if (const std::optional<Local> local = localOf(job.game, error)) {
            ours = local->status;
        }
    }
    const std::vector<Sighting> seen = online_();
    std::vector<PeerSaves> found;
    for (const Peer& peer : peers) {
        const auto sighting = std::ranges::find(seen, peer.id, &Sighting::id);
        if (sighting == seen.end()) {
            found.push_back(PeerSaves{.peer = peer,
                                      .standing = Standing::Offline,
                                      .ours = ours,
                                      .theirs = std::nullopt,
                                      .problem = {}});
            continue;
        }
        std::optional<crypto::Bytes> secret = book_.secretOf(peer.id);
        if (!secret) {
            continue;
        }
        std::optional<PeerSaves> standing =
            syncWith(job, peer, Endpoint{sighting->address, sighting->port}, *secret);
        secrets::wipeBytes(secret->data(), secret->size());
        if (standing) {
            found.push_back(std::move(*standing));
        }
    }
    const std::lock_guard lock{mutex_};
    std::vector<PeerSaves>& kept = standings_[job.game.id];
    if (job.peerId.empty()) {
        kept = std::move(found);
        return;
    }
    for (PeerSaves& standing : found) {
        const auto same = std::ranges::find(kept, standing.peer.id, [](const PeerSaves& one) {
            return one.peer.id;
        });
        if (same != kept.end()) {
            *same = std::move(standing);
        } else {
            kept.push_back(std::move(standing));
        }
    }
}

std::optional<PeerSaves> SaveSync::syncWith(const Job& job, const Peer& peer, const Endpoint& at,
                                            const std::vector<std::uint8_t>& secret) {
    PeerSaves standing{.peer = peer,
                       .standing = Standing::Failed,
                       .ours = {},
                       .theirs = std::nullopt,
                       .problem = {}};
    std::string error;
    std::optional<Local> local;
    {
        const std::lock_guard lock{work_};
        local = localOf(job.game, error);
    }
    if (!local) {
        standing.problem = error;
        return standing;
    }
    standing.ours = local->status;
    const PeerClient client{web_, at};
    const PeerClient::Signer signer{book_.id(), secret};
    const wire::SaveAsk ask = askFor(local->key);
    std::optional<net::WebClient::Answer> answer =
        client.call("POST", wire::saveStatusRoute, wire::write(ask), signer, largestNote, error);
    if (answer && answer->status == forbidden) {
        standing.standing = Standing::NotShared;
        return standing;
    }
    if (answer && answer->status == notFound) {
        // The game is not installed there.
        return std::nullopt;
    }
    std::optional<wire::SaveStatus> theirs;
    if (answer && answer->status == ok) {
        theirs = wire::readSaveStatus(answer->body);
    }
    if (!theirs) {
        standing.problem = peer.name + " cannot be asked: " + describe(answer, error);
        lucent::warn("saves", "{} {}: {}", local->key.system, local->key.game, standing.problem);
        return standing;
    }
    standing.theirs = theirs;
    const std::string synced = versions_.syncedWith(local->key, peer.id);
    SyncState state = syncStateOf(local->status.hash, theirs->hash, synced);
    if (state == SyncState::InSync) {
        versions_.synced(local->key, peer.id, local->status.hash);
    } else if (job.pick == Pick::Theirs && !theirs->hash.empty()) {
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
        wire::SaveAsk news = ask;
        news.picked = job.pick == Pick::Ours;
        answer =
            client.call("POST", wire::saveNewsRoute, wire::write(news), signer, largestNote, error);
        if (!answer || answer->status != ok) {
            standing.standing = Standing::Failed;
            standing.problem =
                peer.name + " was not told of the newer saves: " + describe(answer, error);
            return standing;
        }
        lucent::info("saves", "{} {}: told {} of the newer saves here (version {})",
                     local->key.system, local->key.game, peer.name, local->status.counter);
        return standing;
    }
    case SyncState::Pull:
        break;
    }
    standing.standing = Standing::NewerThere;
    if (job.lookOnly) {
        return standing;
    }
    answer =
        client.call("POST", wire::saveGetRoute, wire::write(ask), signer, largestBundle, error);
    std::optional<SaveBundle> bundle;
    if (answer && answer->status == ok) {
        bundle = readBundle(answer->body);
    }
    if (!bundle || bundle->hash() != theirs->hash) {
        standing.standing = Standing::Failed;
        standing.problem = bundle
                               ? peer.name + "'s saves changed while they were taken"
                               : peer.name + "'s saves cannot be had: " + describe(answer, error);
        lucent::warn("saves", "{} {}: {}", local->key.system, local->key.game, standing.problem);
        return standing;
    }
    if (std::string refused = take(job.game, peer, *bundle, *theirs, local->status.hash);
        !refused.empty()) {
        standing.standing = Standing::Failed;
        standing.problem = std::move(refused);
        lucent::warn("saves", "{} {}: {}'s saves not taken: {}", local->key.system, local->key.game,
                     peer.name, standing.problem);
        return standing;
    }
    standing.standing = Standing::InSync;
    standing.ours = *theirs;
    wire::SaveAsk record = ask;
    record.hash = theirs->hash;
    record.counter = theirs->counter;
    answer =
        client.call("POST", wire::saveSyncedRoute, wire::write(record), signer, largestNote, error);
    if (!answer || answer->status != ok) {
        lucent::warn("saves", "{} was not told its saves were taken: {}", peer.name,
                     describe(answer, error));
    }
    return standing;
}

std::string SaveSync::take(const library::Game& game, const Peer& peer, const SaveBundle& bundle,
                           const wire::SaveStatus& theirs, const std::string& expected) {
    const std::lock_guard lock{work_};
    std::string error;
    const std::optional<Local> local = localOf(game, error);
    if (!local) {
        return error;
    }
    if (local->status.hash != expected) {
        return "the saves here changed meanwhile";
    }
    if (std::string refused = setup_.inUse(game, local->launched); !refused.empty()) {
        return refused;
    }
    const std::string name =
        secrets::hexText(crypto::sha256(local->key.system + "\n" + local->key.game))
            .substr(0, stagingNameBytes * 2);
    const fs::path folder = setup_.staging / peer.id / name;
    library::saves::CopyProgress progress;
    try {
        const library::saves::EmulatorSaves from = staged(bundle, folder, peer.name);
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
    versions_.synced(local->key, peer.id, theirs.hash);
    lucent::info("saves", "{} {}: took {}'s saves (version {}, {} bytes) over {}'s",
                 local->key.system, local->key.game, peer.name, theirs.counter, theirs.bytes,
                 local->launched.emulatorName);
    return {};
}

} // namespace opensu::lan

#include "save_versions.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "wire.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::string nameOf(const library::saves::SaveKey& key) {
    return key.system + "\n" + key.game;
}

} // namespace

SyncState syncStateOf(std::string_view ours, std::string_view theirs,
                      std::string_view synced) noexcept {
    if (ours == theirs) {
        return SyncState::InSync;
    }
    if (ours.empty()) {
        return SyncState::Pull;
    }
    if (theirs.empty()) {
        return SyncState::Push;
    }
    const bool oursChanged = ours != synced;
    const bool theirsChanged = theirs != synced;
    if (oursChanged && theirsChanged) {
        return SyncState::Conflict;
    }
    return oursChanged ? SyncState::Push : SyncState::Pull;
}

SaveVersions::SaveVersions(fs::path file) : file_{std::move(file)} {
    std::error_code ec;
    if (!fs::is_regular_file(file_, ec)) {
        return;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains("games") || !document["games"].is_array()) {
        lucent::warn("saves", "{} is not openSU's record of LAN saves; starting a new one",
                     file_.string());
        return;
    }
    for (const json& entry : document["games"]) {
        if (!entry.is_object() || !entry.contains("counter") ||
            !entry["counter"].is_number_unsigned()) {
            continue;
        }
        Game game{.version = SaveVersion{entry["counter"].get<std::uint64_t>(),
                                         wire::textField(entry, "hash")},
                  .synced = {}};
        if (const auto peers = entry.find("synced"); peers != entry.end() && peers->is_object()) {
            for (const auto& [peer, hash] : peers->items()) {
                if (hash.is_string()) {
                    game.synced.emplace(peer, hash.get<std::string>());
                }
            }
        }
        games_.insert_or_assign(wire::textField(entry, "system") + "\n" +
                                    wire::textField(entry, "game"),
                                std::move(game));
    }
}

SaveVersion SaveVersions::observe(const library::saves::SaveKey& key, const std::string& hash) {
    const std::lock_guard lock{mutex_};
    Game& game = games_[nameOf(key)];
    if (game.version.hash != hash) {
        game.version.hash = hash;
        ++game.version.counter;
        keep();
    }
    return game.version;
}

std::string SaveVersions::syncedWith(const library::saves::SaveKey& key,
                                     std::string_view peer) const {
    const std::lock_guard lock{mutex_};
    const auto game = games_.find(nameOf(key));
    if (game == games_.end()) {
        return {};
    }
    const auto found = game->second.synced.find(peer);
    return found == game->second.synced.end() ? std::string{} : found->second;
}

void SaveVersions::took(const library::saves::SaveKey& key, const std::string& hash,
                        std::uint64_t counter) {
    const std::lock_guard lock{mutex_};
    Game& game = games_[nameOf(key)];
    if (game.version.hash != hash) {
        game.version.hash = hash;
        game.version.counter = std::max(game.version.counter + 1, counter);
        keep();
    }
}

void SaveVersions::synced(const library::saves::SaveKey& key, const std::string& peer,
                          const std::string& hash) {
    const std::lock_guard lock{mutex_};
    Game& game = games_[nameOf(key)];
    const auto [at, added] = game.synced.try_emplace(peer, hash);
    if (added || at->second != hash) {
        at->second = hash;
        keep();
    }
}

void SaveVersions::keep() const {
    json games = json::array();
    for (const auto& [name, game] : games_) {
        const std::size_t split = name.find('\n');
        json synced = json::object();
        for (const auto& [peer, hash] : game.synced) {
            synced[peer] = hash;
        }
        games.push_back({{"system", name.substr(0, split)},
                         {"game", name.substr(split + 1)},
                         {"counter", game.version.counter},
                         {"hash", game.version.hash},
                         {"synced", std::move(synced)}});
    }
    std::string error;
    if (!fileio::writeWhole(file_, json{{"games", std::move(games)}}.dump(2) + "\n", error)) {
        lucent::warn("saves", "the record of LAN saves cannot be kept: {}", error);
    }
}

} // namespace opensu::lan

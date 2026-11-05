#include "save_versions.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "json_text.hpp"
#include "lucent/log.h"

namespace opensu::save_sync {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::string nameOf(std::string_view profile, std::string_view system, std::string_view game) {
    return std::string{profile} + "\n" + std::string{system} + "\n" + std::string{game};
}

std::string nameOf(const library::saves::SaveKey& key) {
    return nameOf(key.profile, key.system, key.game);
}

} // namespace

std::string write(const SaveStatus& status) {
    return json{{"counter", status.counter},
                {"hash", status.hash},
                {"bytes", status.bytes},
                {"modified", status.modified}}
        .dump();
}

std::optional<SaveStatus> readSaveStatus(std::string_view text) {
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        return std::nullopt;
    }
    const auto counter = document.find("counter");
    const auto bytes = document.find("bytes");
    const auto modified = document.find("modified");
    if (counter == document.end() || !counter->is_number_unsigned() || bytes == document.end() ||
        !bytes->is_number_unsigned() || modified == document.end() ||
        !modified->is_number_integer()) {
        return std::nullopt;
    }
    return SaveStatus{counter->get<std::uint64_t>(), textField(document, "hash"),
                      bytes->get<std::uint64_t>(), modified->get<std::int64_t>()};
}

SyncState stateOf(bool oursChanged, bool theirsChanged) noexcept {
    if (oursChanged && theirsChanged) {
        return SyncState::Conflict;
    }
    if (oursChanged) {
        return SyncState::Push;
    }
    return theirsChanged ? SyncState::Pull : SyncState::InSync;
}

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
    return stateOf(ours != synced, theirs != synced);
}

SaveVersions::SaveVersions(fs::path file) : file_{std::move(file)} {
    std::error_code ec;
    if (!fs::is_regular_file(file_, ec)) {
        return;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains("games") || !document["games"].is_array()) {
        lucent::warn("saves", "{} is not openSU's record of shared saves; starting a new one",
                     file_.string());
        return;
    }
    for (const json& entry : document["games"]) {
        if (!entry.is_object() || !entry.contains("counter") ||
            !entry["counter"].is_number_unsigned()) {
            continue;
        }
        Game game{.version =
                      SaveVersion{entry["counter"].get<std::uint64_t>(), textField(entry, "hash")},
                  .synced = {}};
        if (const auto peers = entry.find("synced"); peers != entry.end() && peers->is_object()) {
            for (const auto& [peer, hash] : peers->items()) {
                if (hash.is_string()) {
                    game.synced.emplace(peer, hash.get<std::string>());
                }
            }
        }
        games_.insert_or_assign(nameOf(textField(entry, "profile"), textField(entry, "system"),
                                       textField(entry, "game")),
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
                                     std::string_view target) const {
    const std::lock_guard lock{mutex_};
    const auto game = games_.find(nameOf(key));
    if (game == games_.end()) {
        return {};
    }
    const auto found = game->second.synced.find(target);
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

void SaveVersions::synced(const library::saves::SaveKey& key, const std::string& target,
                          const std::string& hash) {
    const std::lock_guard lock{mutex_};
    Game& game = games_[nameOf(key)];
    const auto [at, added] = game.synced.try_emplace(target, hash);
    if (added || at->second != hash) {
        at->second = hash;
        keep();
    }
}

void SaveVersions::adopt(std::string_view profile) {
    const std::lock_guard lock{mutex_};
    bool changed = false;
    for (auto at = games_.begin(); at != games_.end();) {
        if (!at->first.starts_with('\n')) {
            ++at;
            continue;
        }
        games_.try_emplace(std::string{profile} + at->first, std::move(at->second));
        at = games_.erase(at);
        changed = true;
    }
    if (changed) {
        keep();
    }
}

void SaveVersions::keep() const {
    json games = json::array();
    for (const auto& [name, game] : games_) {
        const std::size_t split = name.find('\n');
        const std::size_t second = name.find('\n', split + 1);
        json synced = json::object();
        for (const auto& [peer, hash] : game.synced) {
            synced[peer] = hash;
        }
        games.push_back({{"profile", name.substr(0, split)},
                         {"system", name.substr(split + 1, second - split - 1)},
                         {"game", name.substr(second + 1)},
                         {"counter", game.version.counter},
                         {"hash", game.version.hash},
                         {"synced", std::move(synced)}});
    }
    std::string error;
    if (!fileio::writeWhole(file_, json{{"games", std::move(games)}}.dump(2) + "\n", error)) {
        lucent::warn("saves", "the record of shared saves cannot be kept: {}", error);
    }
}

} // namespace opensu::save_sync

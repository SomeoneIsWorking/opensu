#include "library/steam_tags.hpp"

#include <iterator>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "fileio/read_whole.hpp"
#include "library/steam_items.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

using nlohmann::json;

/// Apps asked of `GetItems` at once.
constexpr std::size_t batch = 50;
std::map<std::uint32_t, std::vector<std::uint32_t>> load(const std::filesystem::path& file) {
    std::map<std::uint32_t, std::vector<std::uint32_t>> known;
    std::error_code exists;
    if (!std::filesystem::exists(file, exists)) {
        return known;
    }
    std::string error;
    const std::optional<std::string> text = fileio::readWhole(file, error);
    const json document = text ? json::parse(*text, nullptr, false) : json{};
    if (!document.is_object()) {
        lucent::warn("library", "Steam tags in {} cannot be read; asking again", file.string());
        return known;
    }
    for (const auto& [app, tags] : document.items()) {
        const std::optional<std::uint32_t> number = steamAppNumber(app);
        if (!number || !tags.is_array()) {
            continue;
        }
        std::vector<std::uint32_t>& kept = known[*number];
        for (const json& tag : tags) {
            if (tag.is_number_unsigned()) {
                kept.push_back(tag.get<std::uint32_t>());
            }
        }
    }
    return known;
}

} // namespace

SteamTags::SteamTags(std::filesystem::path file, std::string api)
    : file_{std::move(file)}, api_{std::move(api)}, known_{load(file_)},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

SteamTags::~SteamTags() {
    worker_.request_stop();
}

void SteamTags::want(const std::vector<Game>& games) {
    bool added = false;
    {
        const std::lock_guard lock{mutex_};
        for (const Game& game : games) {
            if (const std::optional<std::uint32_t> app = steamAppOf(game);
                app && !known_.contains(*app)) {
                added = wanted_.insert(*app).second || added;
            }
        }
    }
    if (added) {
        wake_.notify_all();
    }
}

std::optional<std::vector<std::uint32_t>> SteamTags::of(std::uint32_t app) const {
    const std::lock_guard lock{mutex_};
    const auto found = known_.find(app);
    return found == known_.end() ? std::nullopt : std::optional{found->second};
}

void SteamTags::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        std::vector<std::uint32_t> apps;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !wanted_.empty();
                })) {
                return;
            }
            auto end = wanted_.begin();
            std::advance(end, std::min(batch, wanted_.size()));
            apps.assign(wanted_.begin(), end);
            wanted_.erase(wanted_.begin(), end);
        }
        const net::Result<std::string> answer = web_.get(stop, steam_items::itemsUrl(apps, api_));
        if (!answer) {
            // Asked again at the next `want`.
            lucent::warn("library", "Steam tags: {}", answer.error());
            continue;
        }
        std::vector<steam_items::Item> items;
        try {
            items = steam_items::parseItems(*answer);
        } catch (const std::runtime_error& error) {
            lucent::warn("library", "Steam tags: {}", error.what());
            continue;
        }
        {
            const std::lock_guard lock{mutex_};
            for (const std::uint32_t app : apps) {
                // An app Steam no longer lists has none, and is not asked again.
                known_[app] = {};
            }
            for (const steam_items::Item& item : items) {
                known_[item.app] = item.tags;
            }
        }
        save();
    }
}

void SteamTags::save() const {
    json document = json::object();
    {
        const std::lock_guard lock{mutex_};
        for (const auto& [app, tags] : known_) {
            document[std::to_string(app)] = tags;
        }
    }
    if (std::string error; !fileio::writeWhole(file_, document.dump() + "\n", error)) {
        lucent::warn("library", "Steam tags cannot be kept: {}", error);
    }
}

} // namespace opensu::library

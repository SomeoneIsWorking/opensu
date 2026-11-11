#include "shadps4_config.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

/// shadPS4's default user for `slot` (`UserManager::CreateDefaultUsers`), `id` its user id.
json defaultUser(int slot, std::int64_t id) {
    return json{
        {"player_index", slot},
        {"shadnet_email", ""},
        {"shadnet_enabled", false},
        {"shadnet_npid", ""},
        {"shadnet_password", ""},
        {"shadnet_token", ""},
        {"user_color", slot},
        {"user_id", id},
        {"user_name", slot == 1 ? std::string{"shadPS4"} : "shadPS4-" + std::to_string(slot)}};
}

} // namespace

std::string shadps4LaunchUsers(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("Users") ||
        !root["Users"].is_object() || !root["Users"].contains("user") ||
        !root["Users"]["user"].is_array()) {
        return std::string{text};
    }
    json& users = root["Users"]["user"];
    std::int64_t nextId = 1000;
    for (const json& user : users) {
        if (user.contains("user_id") && user["user_id"].is_number_integer()) {
            nextId = std::max(nextId, user["user_id"].get<std::int64_t>() + 1);
        }
    }
    bool added = false;
    for (int slot = 1; slot <= shadps4Slots; ++slot) {
        const bool taken = std::ranges::any_of(users, [slot](const json& user) {
            return user.contains("player_index") && user["player_index"] == slot;
        });
        if (!taken) {
            users.push_back(defaultUser(slot, nextId++));
            lucent::info("shadps4", "pad slot {} given a user for the launch", slot);
            added = true;
        }
    }
    return added ? root.dump(2) : std::string{text};
}

std::vector<SettingsWrite> shadps4Writes(const LaunchSetup& setup) {
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / shadps4UsersFile, shadps4LaunchUsers);
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library

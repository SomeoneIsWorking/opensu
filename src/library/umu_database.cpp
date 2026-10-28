#include "umu_database.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::umu {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::string lowered(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The database's name for the store `game` came from; empty for one it does not list.
std::string_view storeOf(const Game& game) {
    switch (game.source) {
    case Source::Gog:
        return "gog";
    case Source::Epic:
        return "egs";
    default:
        return {};
    }
}

/// The ids `listing` maps; throws std::runtime_error when it is not the database's list.
Database::Ids idsOf(const json& listing) {
    if (!listing.is_array()) {
        throw std::runtime_error{"the umu database is not a list"};
    }
    Database::Ids ids;
    for (const json& entry : listing) {
        if (!entry.is_object()) {
            continue;
        }
        const auto field = [&entry](const char* name) {
            const auto found = entry.find(name);
            return found != entry.end() && found->is_string() ? found->get<std::string>()
                                                              : std::string{};
        };
        std::string id = field("umu_id");
        const std::string store = lowered(field("store"));
        const std::string codename = lowered(field("codename"));
        if (id.empty() || store.empty() || codename.empty() || codename == "none") {
            continue;
        }
        ids.try_emplace({store, codename}, std::move(id));
    }
    return ids;
}

bool isAppId(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

} // namespace

fs::path databaseFile(const fs::path& dataDir) {
    return dataDir / "wine" / "umu-database.json";
}

Database Database::parse(std::string_view text) {
    return Database{idsOf(json::parse(text, nullptr, false))};
}

Database Database::read(const fs::path& file) {
    std::error_code error;
    if (!fs::is_regular_file(file, error)) {
        return {};
    }
    std::ifstream in{file, std::ios::binary};
    try {
        return Database{idsOf(json::parse(in, nullptr, false))};
    } catch (const std::runtime_error& refused) {
        lucent::warn("wine", "{} is not used: {}", file.string(), refused.what());
        return {};
    }
}

std::string Database::gameIdOf(const Game& game) const {
    if (game.source == Source::Own && isAppId(game.artworkKey)) {
        return "umu-" + game.artworkKey;
    }
    const std::string_view store = storeOf(game);
    if (store.empty()) {
        return std::string{unknownGameId};
    }
    const auto found = ids_.find(std::pair{std::string{store}, lowered(game.sourceId)});
    return found != ids_.end() ? found->second : std::string{unknownGameId};
}

} // namespace opensu::library::umu

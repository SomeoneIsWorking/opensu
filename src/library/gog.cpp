#include "gog.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace iideck::library::gog {
namespace {

using nlohmann::json;

/// The first non-empty value, or nothing.
/// The first value that is not empty.
template <typename... Args> std::string firstNonEmpty(Args&&... args) {
    const std::array<std::string_view, sizeof...(Args)> values{std::string_view{args}...};
    for (const std::string_view value : values) {
        if (!value.empty()) {
            return std::string{value};
        }
    }
    return {};
}

/// Heroic nests the display fields under "library" and keeps install state
/// alongside; the nested values are the ones it shows.
struct LibraryEntry {
    std::string title;
    std::string appName;
    std::string installPath;
    bool installed{false};

    [[nodiscard]] static LibraryEntry from(const json& entry) {
        const json lib = entry.contains("library") ? entry["library"] : json::object();
        LibraryEntry out;
        out.title = firstNonEmpty(lib.value("title", ""), entry.value("title", ""));
        out.appName = firstNonEmpty(lib.value("appName", ""), entry.value("app_name", ""),
                                    std::string_view{out.title});
        if (entry.contains("install") && entry["install"].is_object()) {
            const json& install = entry["install"];
            out.installPath = install.value("path", "");
            out.installed = install.value("installed", false);
        }
        return out;
    }
};

} // namespace

Provider::Provider(const std::filesystem::path& home)
    : configDir_{home / ".config" / "heroic"}, binary_{"heroic"} {
}

std::vector<Game> Provider::list() {
    std::error_code ec;
    if (!std::filesystem::is_directory(configDir_, ec)) {
        throw std::runtime_error{"heroic is not installed"};
    }
    // Heroic caches its library under store_cache as plain JSON. No file yet
    // means no library saved, which is not a failure.
    const std::filesystem::path cache = configDir_ / "store_cache" / "gog_library.json";
    if (!std::filesystem::exists(cache, ec)) {
        return {};
    }

    const json document = json::parse(std::ifstream{cache});
    // Heroic writes "{}" for an empty library and "[]" once titles exist.
    if (document.is_object() && document.empty()) {
        return {};
    }
    if (!document.is_array()) {
        throw std::runtime_error{"heroic library has an unexpected shape"};
    }

    std::vector<Game> games;
    for (const json& entry : document) {
        const LibraryEntry parsed = LibraryEntry::from(entry);
        if (parsed.title.empty() || parsed.appName.empty()) {
            continue;
        }
        Game game;
        game.id = "gog:" + parsed.appName;
        game.source = Source::Gog;
        game.sourceId = parsed.appName;
        game.title = parsed.title;
        // Heroic keeps its own record of what is installed.
        game.installed = parsed.installed;
        game.launch = LaunchSpec{
            .program = binary_,
            .args = {"util", "install", "--platform", "gog", parsed.appName},
        };
        if (!parsed.installPath.empty()) {
            game.processHint = std::filesystem::path{parsed.installPath}.filename().string();
        }
        games.push_back(std::move(game));
    }
    lucent::info("gog", "heroic listed {} titles", games.size());
    return games;
}

} // namespace iideck::library::gog
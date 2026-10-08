#include "epic.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "launch/command.hpp"
#include "lucent/log.h"

namespace iideck::library::epic {
namespace {

using nlohmann::json;

/// One owned title from `legendary list --json`.
struct Owned {
    std::string appName;
    std::string title;
    std::string artworkUrl;
};

/// One installed title from `legendary list-installed --json`.
struct Install {
    std::string installPath;
    bool dlc{false};
};

/// Key image types in preference order: the portrait box, then the wide box, then the thumbnail.
constexpr std::array<std::string_view, 3> imageTypes{"DieselGameBoxTall", "DieselGameBox",
                                                     "Thumbnail"};

/// The URL of the preferred key image in `metadata.keyImages`; empty when there is none.
std::string artworkUrl(const json& entry) {
    const json::const_iterator metadata = entry.find("metadata");
    if (metadata == entry.end() || !metadata->is_object()) {
        return {};
    }
    const json::const_iterator images = metadata->find("keyImages");
    if (images == metadata->end() || !images->is_array()) {
        return {};
    }
    for (const std::string_view type : imageTypes) {
        for (const json& image : *images) {
            if (image.value("type", "") == type && image.value("url", "").starts_with("https://")) {
                return image.value("url", "");
            }
        }
    }
    return {};
}

std::vector<Owned> parseOwned(std::string_view output) {
    std::vector<Owned> owned;
    for (const json& entry : json::parse(output)) {
        owned.push_back(Owned{.appName = entry.value("app_name", ""),
                              .title = entry.value("app_title", ""),
                              .artworkUrl = artworkUrl(entry)});
    }
    return owned;
}

std::map<std::string, Install, std::less<>> parseInstalls(std::string_view output) {
    std::map<std::string, Install, std::less<>> installs;
    for (const json& entry : json::parse(output)) {
        installs[entry.value("app_name", "")] = Install{
            .installPath = entry.value("install_path", ""), .dlc = entry.value("is_dlc", false)};
    }
    return installs;
}

constexpr std::string_view progressMarker = "= Progress: ";
constexpr std::string_view failureMarker = " ! Failure: ";

/// `line` without a trailing carriage return.
std::string_view trimmed(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.remove_suffix(1);
    }
    return line;
}

} // namespace

std::optional<double> installProgress(std::string_view line) {
    const std::size_t at = line.find(progressMarker);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view number = line.substr(at + progressMarker.size());
    double percent = 0.0;
    const auto [end, error] =
        std::from_chars(number.data(), number.data() + number.size(), percent);
    if (error != std::errc{} || end == number.data() + number.size() || *end != '%') {
        return std::nullopt;
    }
    return std::clamp(percent / 100.0, 0.0, 1.0);
}

std::optional<std::string> installFailure(std::string_view line) {
    line = trimmed(line);
    for (const std::string_view level : {"ERROR: ", "CRITICAL: "}) {
        // "[cli] ERROR: message": the logger's name in brackets, then the level.
        const std::size_t at = line.find(level);
        if (line.starts_with('[') && at != std::string_view::npos && line.find(']') + 2 == at) {
            return std::string{line.substr(at + level.size())};
        }
    }
    if (line.starts_with(failureMarker)) {
        return std::string{line.substr(failureMarker.size())};
    }
    return std::nullopt;
}

std::string Provider::run(const std::vector<std::string>& arguments) const {
    // Legendary logs to stderr, so only stdout is captured.
    const std::optional<launch::Captured> result = launch::runCaptured(binary_, arguments);
    if (!result) {
        throw SourceAbsent{"legendary is not installed"};
    }
    if (result->status != 0) {
        // Legendary exits non-zero when it has no saved credentials.
        throw std::runtime_error{"legendary is not logged in"};
    }
    return result->output;
}

Provider::Provider() : binary_{"legendary"} {
}

Provider::Provider(std::string binary) : binary_{std::move(binary)} {
}

std::vector<Game> Provider::list() {
    const std::vector<Owned> owned = parseOwned(run({"list", "--json"}));
    const auto installs = parseInstalls(run({"list-installed", "--json"}));

    std::vector<Game> games;
    for (const Owned& title : owned) {
        const auto install = installs.find(title.appName);
        // DLC is not a tile of its own; it belongs to its base game.
        if (title.appName.empty() || (install != installs.end() && install->second.dlc)) {
            continue;
        }
        Game game;
        game.id = "epic:" + title.appName;
        game.source = Source::Epic;
        game.sourceId = title.appName;
        game.title = title.title.empty() ? title.appName : title.title;
        game.artworkUrl = title.artworkUrl;
        game.installed = install != installs.end();
        game.launch = LaunchSpec{.program = "legendary", .args = {"launch", title.appName}};
        if (game.installed && !install->second.installPath.empty()) {
            // The install folder name identifies the running game.
            game.processHint =
                std::filesystem::path{install->second.installPath}.filename().string();
        }
        games.push_back(std::move(game));
    }
    lucent::info("epic", "legendary listed {} owned titles, {} installed", games.size(),
                 installs.size());
    return games;
}

} // namespace iideck::library::epic